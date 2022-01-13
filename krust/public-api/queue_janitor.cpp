// Copyright (c) 2022 Andrew Helge Cox
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/**
 * @file ...
 */

// Internal includes:
#include "krust/public-api/queue_janitor.h"
#include "krust/public-api/krust-errors.h"
#include "krust/public-api/thread-base.h"
#include "krust/internal/keep-alive-set.h"
#include "krust/internal/scoped-temp-array.h"
#include "krust/public-api/vulkan.h"

// External includes:

namespace Krust
{

/// A bundle of handles to the CPU-side representations of objects used on the GPU
/// during a submit's execution.
struct QueueJanitor::SubmitLiveBatch {
  bool InFlight(){
    return liveCommandbuffers.size() > 0;
  }
  void KeepAlive(const span<QueueSubmitInfo, dynamic_extent> submits);
  /// Command buffers that we will inform when a batch of submits completes on the GPU.
  std::vector<CommandBufferPtr> liveCommandbuffers;
  /// All semaphores and possible future resources used by a submit are added in here.
  KeepAliveSet liveOthers;
  /// Points to Fence which is passed into the Vulkan submit function.
  FencePtr completionSignal;
  /// Which submit in monotonically increasing order for the associated queue this
  /// batch represents.
  SubmitCounter submitCounter = 0;
};

void QueueJanitor::SubmitLiveBatch::KeepAlive(const span<QueueSubmitInfo, dynamic_extent> submits)
{
  for(const auto& submit : submits)
  {
    liveCommandbuffers.insert(liveCommandbuffers.end(), submit.commandBuffers.begin(), submit.commandBuffers.end());
    for(const auto& wait : submit.waits){
      liveOthers.Add(*wait.first);
    }
    for(const auto& semi : submit.completionSignals){
      liveOthers.Add(*semi);
    }
  }
}

// -----------------------------------------------------------------------------
QueueJanitor::QueueJanitor(Device& device, const uint32_t queueFamilyIndex, const uint32_t queueIndex)
    : mQueue(Queue::New(device, queueFamilyIndex, queueIndex))
{
  // If we are doing error reporting with exceptions, the following check is redundant / never reached.
  if(nullptr == mQueue.Get())
  {
    ThreadBase::Get().GetErrorPolicy().Error(Krust::Errors::IllegalState, "Returned a null Queue pointer.", __FUNCTION__, __FILE__, __LINE__);
  }
  mLiveBatches = new SubmitLiveBatches();
}

QueueJanitorPtr QueueJanitor::New(Device& device, const uint32_t queueFamilyIndex, const uint32_t queueIndex)
{
  QueueJanitorPtr janitor = new QueueJanitor(device, queueFamilyIndex, queueIndex);
  // In case we have been compiled without exceptions and/or have a non-exceptions
  // error policy enabled, return a null pointer to indicate error if the
  // constructor failed. Even then we assume allocation can't fail or we'd have to
  // check any of the Janitor's members that are heap allocated too.
  // If we are using exceptions, in this error state we'd have thrown already, so
  // this can never trigger and is wasted code.
  if(janitor.Get() && (janitor->mQueue.Get() == nullptr)){
    janitor.Reset();
  }
  return janitor;
}

QueueJanitor::~QueueJanitor()
{
  mQueue.Reset();
  delete mLiveBatches;
  mLiveBatches = nullptr;
}

void QueueJanitor::CheckCompletions()
{
  for(SubmitLiveBatch& b : *mLiveBatches){
    if(b.InFlight()){
      Fence* fence = b.completionSignal.Get();
      VkResult wait_res = vkWaitForFences(mQueue->GetDevice(), 1, fence->GetVkFenceAddress(), true, 0);
      if(wait_res == VK_SUCCESS){
        this->RecycleLiveBatch(b);
      } else if(wait_res == VK_ERROR_DEVICE_LOST){
        KRUST_LOG_ERROR << "VK_ERROR_DEVICE_LOST calling vkWaitForFences() from " << __FUNCTION__ << " in File " __FILE__ " at line " << __LINE__  << endlog;
        mLiveBatches->clear();
        break;
      }
    }
  };
}

QueueJanitor::SubmitLiveBatch& QueueJanitor::GetLiveBatch()
{
  SubmitLiveBatch* batch = nullptr;
  for(SubmitLiveBatch& b : *mLiveBatches){
    if(!b.InFlight()){
      batch = &b;
      break;
    }
  }
  if(batch == nullptr){
    mLiveBatches->resize(mLiveBatches->size() + 1);
    batch = & mLiveBatches->back();
  }
  return *batch;
}

void QueueJanitor::RecycleLiveBatch(QueueJanitor::SubmitLiveBatch& batch)
{
  batch.liveCommandbuffers.clear();
  batch.liveOthers.Clear();
  vkResetFences(mQueue->GetDevice(), 1, batch.completionSignal->GetVkFenceAddress());
}

std::array<size_t, 3> CountSubmits(span<QueueSubmitInfo, dynamic_extent> submits)
{
  size_t buffers = 0;
  size_t semaphores = 0;
  size_t wait_flags = 0;

  for(auto& submit : submits)
  {
    wait_flags += submit.waits.size();
    semaphores += submit.waits.size() + submit.completionSignals.size();
    buffers += submit.commandBuffers.size();
  }
  return {buffers, semaphores, wait_flags};
}

SubmitResult QueueJanitor::Submit(span<QueueSubmitInfo, dynamic_extent> submits)
{
  VkResult result = VK_ERROR_UNKNOWN;
  auto submitCounter = mNextSubmit++;

  // Translate the submit infos into the native Vulkan struct type:
  auto [buffers, semaphores, wait_flags] = CountSubmits(submits);
  ScopedTempArray<VkSubmitInfo, sizeof(VkSubmitInfo) * 10>
    vk_submits(submits.size());
  ScopedTempArray<VkSemaphore, sizeof(VkSemaphore) * 20>
    vk_semaphores(semaphores);
  ScopedTempArray<VkPipelineStageFlags, sizeof(VkPipelineStageFlags) * 10>
    vk_wait_flags(wait_flags);
  ScopedTempArray<VkCommandBuffer, sizeof(VkCommandBuffer) * 10> 
    vk_buffers(buffers);
  VkSemaphore*          next_semi = vk_semaphores.Get();
  VkCommandBuffer*      next_buffer = vk_buffers.Get();
  VkPipelineStageFlags* next_wait_flags = vk_wait_flags.Get();

  for(size_t submit_i = 0; submit_i < submits.size(); ++submit_i)
  {
    QueueSubmitInfo& submit = submits[submit_i];
    VkSubmitInfo&   vk_submit = vk_submits.Get()[submit_i];
    vk_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vk_submit.pNext = submit.pNext;

    vk_submit.waitSemaphoreCount   = submit.waits.size();
    vk_submit.commandBufferCount   = submit.commandBuffers.size();
    vk_submit.signalSemaphoreCount = submit.completionSignals.size();
    vk_submit.pWaitSemaphores = next_semi;
    vk_submit.pWaitDstStageMask = next_wait_flags;
    vk_submit.pCommandBuffers = next_buffer;
    vk_submit.pSignalSemaphores = next_semi + vk_submit.waitSemaphoreCount;
    for(size_t i = 0; i < submit.waits.size(); ++i)
    {
      *next_semi++ = *(submit.waits[i].first);
      *next_wait_flags++ = submit.waits[i].second;
    }
    for(size_t i = 0; i < submit.completionSignals.size(); ++i)
    {
      *next_semi++ = *submit.completionSignals[i];
    }
    for(size_t i = 0; i < submit.commandBuffers.size(); ++i)
    {
      *next_buffer++ = *submit.commandBuffers[i];
    }
  }
  // See if any previous submits have finished, and release their resources:
  CheckCompletions();
  // Get a record to keep resource alive on the host while they are in use on the device:
  SubmitLiveBatch& live_batch = GetLiveBatch();
  live_batch.submitCounter = submitCounter;
  live_batch.KeepAlive(submits);
  result = vkQueueSubmit(*mQueue, submits.size(), vk_submits.Get(), *live_batch.completionSignal);
  return {result, submitCounter};
}

} /* namespace Krust */
