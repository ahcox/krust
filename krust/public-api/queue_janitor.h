#ifndef KRUST_COMMON_PUBLIC_API_QUEUE_JANITOR_H
#define KRUST_COMMON_PUBLIC_API_QUEUE_JANITOR_H

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
 * @file Facilities for remembering which resources are in-use on the GPU for
 * the duration of a submission's processing.
 */

// Internal includes:
#include "krust/public-api/vulkan-objects.h"
#include "krust-kernel/public-api/span.h"

// External includes:
#include <vector>

namespace Krust
{

class QueueJanitor;
using QueueJanitorPtr = IntrusivePointer<QueueJanitor>;
using SubmitCounter = uint64_t;

/// Wraps the result of a vulkan queue submission and a counter for the
/// submission. The counter can be used later to query for completion.
class SubmitResult {
public:
    SubmitResult(VkResult vkResult, SubmitCounter submitCounter) : submitCounter(submitCounter), vkResult(vkResult) {}
    SubmitCounter counter() const { return submitCounter; }
    VkResult result() const { return vkResult; }
private:
    const SubmitCounter submitCounter;
    const VkResult vkResult;
};

/// Describes the data required for a queue submission.
struct QueueSubmitInfo {
  QueueSubmitInfo(
    span<std::pair<SemaphorePtr, const VkPipelineStageFlags>, dynamic_extent> waits,
    span<CommandBufferPtr, dynamic_extent>                                    commandBuffers,
    span<SemaphorePtr, dynamic_extent>                                        completionSignals
  ) : waits(waits), commandBuffers(commandBuffers), completionSignals(completionSignals)
  {}
  /// Use to extend the submit as you would for pNext of VkSubmitInfo.
  const void* pNext = nullptr;
  span<std::pair<SemaphorePtr, const VkPipelineStageFlags>, dynamic_extent> waits;
  span<CommandBufferPtr, dynamic_extent>                                    commandBuffers;
  span<SemaphorePtr, dynamic_extent>                                        completionSignals;
};

/* ----------------------------------------------------------------------- *//**
 * @brief A wrapper for Vulkan's Queue API object which keeps related API
 * objects alive on the CPU while in use on the GPU.
 */
class QueueJanitor : public RefObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  QueueJanitor(Device& device, const uint32_t queueFamilyIndex, const uint32_t queueIndex);

  // Ban copying objects:
  QueueJanitor(const QueueJanitor&) = delete;
  QueueJanitor& operator=(const QueueJanitor&) = delete;

  struct SubmitLiveBatch;
  using SubmitLiveBatches = std::vector<SubmitLiveBatch>;

public:
  /**
   * @brief Creator for new handles to Queue Janitor objects.
   * @return Smart pointer wrapper to keep the Queue Janitor alive.
   */
  static QueueJanitorPtr New(Device& device, const uint32_t queueFamilyIndex, const uint32_t queueIndex);
  /**
   * Destruction requires waiting for the queue to go idle so the smart pointer
   * to the device can be released.
   */
  ~QueueJanitor();

  /**
   * Wrapper for submission which keeps the CPU-side handles for the submitted
   * command buffers alive while the underlying data structures that they
   * represent are in use on the GPU.
   */
  SubmitResult Submit(span<QueueSubmitInfo, dynamic_extent> submits);

  /**
   * Call this to signal any completion Fences passed to Submit() for completed
   * submits and to free any Command Buffers and Semaphores previously kept alive
   * while in use on the GPU.
   */
  void CheckCompletions();

  operator VkQueue() const { return *mQueue; }
  Queue& GetQueue() const { return *mQueue; }
  Device& GetDevice() const { return mQueue->GetDevice(); }

  SubmitLiveBatch& GetLiveBatch();
  void RecycleLiveBatch(SubmitLiveBatch& batch);

private:
  /// A monotonic counter of submissions.
  SubmitCounter mNextSubmit = 0;
  QueuePtr mQueue = nullptr;
  /// The submitted batches of command buffers "live" in-flight on the GPU.
  SubmitLiveBatches* mLiveBatches = nullptr;
  size_t mNumLiveBatches = 0;
};

} /* namespace Krust */

#endif // KRUST_COMMON_PUBLIC_API_QUEUE_JANITOR_H
