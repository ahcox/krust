// Copyright (c) 2016-2021 Andrew Helge Cox
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

// External includes:
#include "krust-io/public-api/krust-io.h"
#include "krust-gm/public-api/vec3_fwd.h"
#include "krust-gm/public-api/vec3_inl.h"
#include "krust/public-api/krust.h"
#include "krust/public-api/queue_janitor.h"
#include "krust/public-api/line-printer.h"
#include "krust/public-api/device-memory-mapper.h"
#include "krust/public-api/vulkan-utils.h"
#include "krust/public-api/conditional-value.h"
#include "krust-kernel/public-api/floats.h"
#include <chrono>

namespace kr = Krust;

namespace
{

/** Number of samples per framebuffer pixel. */
constexpr VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;
constexpr VkAllocationCallbacks* ALLOCATION_CALLBACKS = nullptr;
constexpr unsigned WORKGROUP_X = 8u;
constexpr unsigned WORKGROUP_Y = 8u;
constexpr const char* const RT1_SHADER = "rt1.comp.spv";
constexpr const char* const RT2_SHADER = "rt2.comp.spv";
constexpr const char* const GREY_SHADER = "rtow_diffuse_grey.comp.spv";
constexpr const char* const MATERIALS_SHADER = "rtow_ray_query.comp.spv";
constexpr const char* const SPHERE_TO_AABB_SHADER_NAME = "spheres_to_aabbs.comp.spv";

struct Pushed
{
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t frame_no;
    float pad0;
    float ray_origin[3];
    float padding1;
    float ray_target_origin[3];
    float padding2;
    float ray_target_right[3];
    float padding3;
    float ray_target_up[3];
    float padding4;
};
KRUST_COMPILE_ASSERT(sizeof(Pushed) <= 128u, "Push Constants are larger than the minimum guaranteed space.")

/// Values which vary between shaders that this app can run.
struct ShaderParams {
  Pushed push_defaults;
  float  move_scale;
};

void viewVecsFromAngles(
  const float pitch, const float yaw,
  kr::Vec3& rightOut, kr::Vec3& upOut, kr::Vec3& fwdOut)
{
  const float cosPitch = cosf(pitch);
  const float cosYaw   = cosf(yaw);
  const float cosRoll  = 1.0f;
  const float sinPitch = sinf(pitch);
  const float sinYaw   = sinf(yaw);
  const float sinRoll  = 0.0f;

  fwdOut = kr::make_vec3(sinYaw * cosPitch, sinPitch, cosPitch * (-cosYaw));
  upOut  = kr::make_vec3(-cosYaw * sinRoll - sinYaw * sinPitch * cosRoll, cosPitch * cosRoll, -sinYaw * sinRoll - sinPitch * cosRoll * -cosYaw);
  rightOut = kr::cross(fwdOut, upOut);
}

using Vec4 = std::experimental::simd<float, std::experimental::simd_abi::fixed_size<4>>;
class Vec4Scalar;

#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

    struct alignas(16) Vec4InMemory {
        float v[4];
    };
#else
    struct Vec4InMemory {
        float v[4];
    };
#endif

inline Vec4 load(const Vec4InMemory& vmem)
{
    Vec4 vec;
    vec.copy_from(&vmem.v[0], std::experimental::overaligned<alignof(Vec4InMemory)>);
    return vec;
}

inline kr::Vec3 xyz(const Vec4& v4){
  return kr::make_vec3(v4[0], v4[1], v4[2]);
}

inline kr::Vec3 xxx(const Vec4& v4){
  return kr::make_vec3(v4[0], v4[0], v4[0]);
}
/// @todo All the swizzles but put them elsewhere.

struct AABBf {
  float minX;
  float minY;
  float minZ;
  float maxX;
  float maxY;
  float maxZ;
};

inline std::vector<AABBf> spheresToAABBs(kr::span<const Vec4InMemory, kr::dynamic_extent> spheres)
{
  using kr::Vec3;

  std::vector<AABBf> aabbs;
  aabbs.resize(spheres.size());
  for(size_t i = 0, end = spheres.size(); i < end; ++i)
  {
    /// @todo Examine the SIMD code generated for this. It might be worse than a scalar version.
    const Vec4 sphere {load(spheres[i])};
    const Vec3 centre = xyz(sphere);
    const auto radius = sphere[3];
    const Vec3 min_corner = centre - radius;
    const Vec3 max_corner = centre + radius;
    kr::store(min_corner, &aabbs[i].minX);
    kr::store(max_corner, &aabbs[i].maxX);
  }
  return aabbs;
}

inline kr::BufferPtr createSingleAllocBuffer(
  kr::Device& device,
  const uint32_t memory_type,
  VkBufferCreateFlags flags,
  VkDeviceSize size,
  VkBufferUsageFlags usage,
  VkSharingMode sharingMode,
  const uint32_t queueFamilyIndex)
{
  auto buffer = kr::Buffer::New(device, flags, size, usage, sharingMode, queueFamilyIndex);
  // Work out how much memory the buffer needs, and then allocate and bind that:
  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device, *buffer, &memReq);
  if(((1u<<memory_type) & memReq.memoryTypeBits) == 0u){
    buffer.Reset();
    KRUST_LOG_ERROR << "Can't put buffer in requested memory_type (" << memory_type << ')' << kr::endlog;
    kr::ThreadBase::Get().GetErrorPolicy().Error(kr::Errors::IllegalArgument, "Buffer not compatible with memory_type requested.", __FUNCTION__, __FILE__, __LINE__);
  } else {
    auto dedicatedInfo = kr::MemoryDedicatedAllocateInfo(VK_NULL_HANDLE, *buffer);
    auto memoryAllocateInfo = kr::MemoryAllocateInfo(memReq.size, memory_type);
    memoryAllocateInfo.pNext = &dedicatedInfo;
    auto memory = kr::DeviceMemory::New(device, memoryAllocateInfo);
    buffer->BindMemory(*memory, 0);
  }
  return buffer;
}

inline kr::BufferPtr createStagingBuffer(
  kr::Device& device,
  const uint32_t memory_type,
  const VkDeviceSize size,
  const uint32_t queueFamilyIndex)
{
  return createSingleAllocBuffer(device,
    memory_type,
    0,
    size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | // To allow staging from host to device.
    VK_BUFFER_USAGE_TRANSFER_DST_BIT,  // To allow staging from device back to host.
    VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
    queueFamilyIndex);
}

/**
 * @return A buffer with the data in staging memory (host visible) if
 * successful, else return a null pointer.*/
inline kr::BufferPtr uploadToStagingBuffer(
  kr::Device& device,
  const VkDeviceSize size,
  const void* hostData,
  const uint32_t memory_type,
  const uint32_t queueFamilyIndex)
{
  auto stagingBuffer = createStagingBuffer(
    device,
    memory_type,
    size,
    queueFamilyIndex);
  if(stagingBuffer.Get())
  {
    kr::DeviceMemoryMapper mapper(stagingBuffer->GetMemory(), 0, size);
    void* stagingData = mapper.GetHostAccess();
    if(stagingData){
      mempcpy(stagingData, hostData, size);
    } else {
      stagingBuffer.Reset();
    }
  }
  return stagingBuffer;
}

/// Copy between buffers.
inline bool CopyBuffer(
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  /// Must have been created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT.
  const kr::Buffer& sourceBuffer,
  /// Must have been created with VK_BUFFER_USAGE_TRANSFER_DST_BIT.
  kr::Buffer& destBuffer,
  VkDeviceSize srcOffset,
  VkDeviceSize dstOffset,
  VkDeviceSize size)
{
  kr::Device& device = sourceBuffer.GetDevice();
  KRUST_ASSERT1(&device == &destBuffer.GetDevice(), "Buffers must belong to same device when copying.");

  auto commandBuffer = kr::CommandBuffer::New(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto beginInfo = kr::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr);
  VkResult result = VK_SUCCESS;
  if(VK_SUCCESS == (result = vkBeginCommandBuffer(*commandBuffer, &beginInfo)))
  {
    const auto region = kr::BufferCopy(srcOffset, dstOffset, size);
    vkCmdCopyBuffer(*commandBuffer, sourceBuffer, destBuffer, 1, &region);
    if(VK_SUCCESS == (result = vkEndCommandBuffer(*commandBuffer)))
    {
      const auto submitInfo = kr::SubmitInfo(0, nullptr, nullptr, 1, commandBuffer->GetVkCommandBufferAddress(), 0, nullptr);
      if(VK_SUCCESS == (result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)))
      {
        if(VK_SUCCESS == (result = vkQueueWaitIdle(queue))) /// @todo Fixme: an idle queue is the last thing we should be creating. <--------------------------------------[FixMe]
        {
          return true;
        }
      }
    }
  }
  KRUST_LOG_ERROR << "Failed to copy buffer with error " << result << kr::endlog;
  return false;
}

/// Copy buffer contents from the start of the source to the start of the destination.
inline bool CopyBuffer(
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  /// Must have been created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT.
  const kr::Buffer& sourceBuffer,
  /// Must have been created with VK_BUFFER_USAGE_TRANSFER_DST_BIT.
  kr::Buffer& destBuffer,
  VkDeviceSize size)
{
  return CopyBuffer(queue, commandPool, sourceBuffer, destBuffer, 0, 0, size);
}

/**
 * Given a buffer in device-visible memory (e.g. staging memory on-device but
 * host visible), return a new buffer in device-visible memory with a copy of
 * the source buffer's contents, doing the copy on the device.
 * @return A buffer with the data in device memory if successful, else return
 * a null pointer.*/
inline kr::BufferPtr transferToDeviceBuffer(
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  /// The buffer in, e.g., "staging" memory.
  kr::Buffer& sourceBuffer,
  VkDeviceSize size,
  /// Extra flags to configure the buffer we create as well as VK_BUFFER_USAGE_TRANSFER_DST_BIT.
  VkBufferUsageFlags bufferUsages,
  /// The type of memory to allocate for the new buffer. This must be device visible.
  const uint32_t memory_type,
  uint32_t queueFamilyIndex)
{
  kr::Device& device = sourceBuffer.GetDevice();
  auto destBuffer = createSingleAllocBuffer(device,
    memory_type,
    0,
    size,
    bufferUsages | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
    queueFamilyIndex);
  if(!destBuffer.Get()){
    KRUST_LOG_ERROR << "Failed to create a new buffer to copy into in function \"" << __FUNCTION__ << "\"." << kr::endlog;
  } else {
    // Initiate a transfer between the buffers:
    if(!CopyBuffer(queue, commandPool, sourceBuffer, *destBuffer, size)){
      destBuffer.Reset();
      KRUST_LOG_ERROR << "Copy of buffer contents failed in \"" << __FUNCTION__ << "\"." << kr::endlog;
    }
  }
  return destBuffer;
}

/**
 * @return A buffer with the data in staging memory (host visible) if
 * successful, else return a null pointer.*/
inline kr::BufferPtr downloadToStagingBuffer(
  kr::Device& device,
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  /// The buffer in, e.g., "staging" memory.
  kr::Buffer& sourceBuffer,
  const VkDeviceSize size,
  /// The type of memory to allocate for the new buffer. This must be device visible and host visible.
  const uint32_t memory_type,
  const uint32_t queueFamilyIndex)
{
  auto stagingBuffer = createStagingBuffer(
    device,
    memory_type,
    size,
    queueFamilyIndex);
  if(stagingBuffer.Get())
  {
    // Initiate a transfer between the buffers:
    if(!CopyBuffer(queue, commandPool, sourceBuffer, *stagingBuffer, size)){
      stagingBuffer.Reset();
      KRUST_LOG_ERROR << "Copy of buffer contents failed in \"" << __FUNCTION__ << "\"." << kr::endlog;
    }
  }
  return stagingBuffer;
}

/// @return A buffer with a dedicated memory allocation backing it, or a null
/// pointer if a step in the process failed.
inline kr::BufferPtr uploadToDeviceBuffer(
  kr::Device& device,
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  VkDeviceSize size,
  /// Extra flags to configure the buffer we create as well as VK_BUFFER_USAGE_TRANSFER_DST_BIT.
  VkBufferUsageFlags bufferUsages,
  const void* hostData,
  const uint32_t staging_memory_type,
  const uint32_t device_memory_type,
  uint32_t queueFamilyIndex)
{
  auto stagingBuffer = uploadToStagingBuffer(device, size, hostData, staging_memory_type, queueFamilyIndex);
  if(!stagingBuffer.Get())
  {
    return nullptr;
  }
  auto deviceBuffer = transferToDeviceBuffer(queue, commandPool, *stagingBuffer, size, bufferUsages, device_memory_type, queueFamilyIndex);
  return deviceBuffer;
}

template<typename T>
inline kr::BufferPtr uploadToDeviceBuffer(
  kr::Device& device,
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  const kr::span<const T, kr::dynamic_extent> hostData,
  /// Extra flags to configure the buffer we create as well as VK_BUFFER_USAGE_TRANSFER_DST_BIT.
  VkBufferUsageFlags bufferUsages,
  const uint32_t staging_memory_type,
  const uint32_t device_memory_type,
  const uint32_t queueFamilyIndex)
{
  return uploadToDeviceBuffer(device, queue, commandPool, hostData.size_bytes(), bufferUsages, hostData.data(), staging_memory_type, device_memory_type, queueFamilyIndex);
}

/// Slow, basic utility for getting data from device to host.
/// @return true if the download worked, else false.
inline bool downloadFromDeviceBuffer(
  kr::Device& device,
  kr::Buffer& buffer,
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  const VkDeviceSize size,
  void* hostData,
  const uint32_t staging_memory_type,
  uint32_t queueFamilyIndex)
{
  auto stagingBuffer = downloadToStagingBuffer(
    device,
    queue,
    commandPool,
    buffer,
    size,
    staging_memory_type,
    queueFamilyIndex);
  if(stagingBuffer.Get()){
    kr::DeviceMemoryMapper mapper(stagingBuffer->GetMemory(), 0, VK_WHOLE_SIZE); // We map the whole buffer to avoid worrying about memory sizes that are not a multiple of VkPhysicalDeviceLimits::nonCoherentAtomSize.
    const void* stagingData = mapper.GetHostAccess();
    if(stagingData){
      mempcpy(hostData, stagingData, size);
      return true;
    }
  }
  return false;
}

template <typename T>
inline bool downloadFromDeviceBuffer(
  kr::Device& device,
  kr::Buffer& buffer,
  /// Queue to schedule the copy on.
  VkQueue queue,
  /// Pool to get a command buffer to run the copy from.
  kr::CommandPool& commandPool,
  kr::span<T, kr::dynamic_extent> hostData,
  const uint32_t staging_memory_type,
  uint32_t queueFamilyIndex)
{
  return downloadFromDeviceBuffer(device, buffer, queue, commandPool, VkDeviceSize{hostData.size_bytes()}, hostData.data(), staging_memory_type, queueFamilyIndex);
}

/// @return A buffer with a dedicated memory allocation backing it and the
/// min/max extents of a set of bounding boxes in i, or a null pointer if a step
/// in the process failed.
/// @note This function has horrendous overheads for running a simple kernel on
/// the GPU. It would be faster to run the algorithm on the CPU ad uploasd the
/// result. Doing things this way does, however, allow evolving towards the most
/// optimal variant that would upload using mostly resources that were created
/// once at startup and reused.
/// @todo FixMe: hoist all this one-time stuff like pipeline creation out of the function so it can be reused to turn many sets of spheres into AABBoxes.
inline kr::BufferPtr spheresToAABBs(
  /// Queue to schedule the upload and compute shader on.
  VkQueue queue,
  /// Pool to get a command buffer to run on.
  kr::CommandPool& commandPool,
  /// Number of spheres in sphereBuffer we care about (byte size is 16 times this).
  const VkDeviceSize numSpheres,
  /// A packed list of x,y,z,radius vec4s representing spheres.
  kr::Buffer& sphereBuffer,
  /// Extra flags to configure the buffer we create as well as VK_BUFFER_USAGE_STORAGE_BUFFER_BIT.
  VkBufferUsageFlags bufferUsages,
  /// The type of memory to put the AABBs in.
  const uint32_t memory_type,
  uint32_t queueFamilyIndex)
{
  kr::Device& device = sphereBuffer.GetDevice();
  const auto aabbBuffersize = numSpheres * sizeof(VkAabbPositionsKHR);
  auto aabbBuffer = createSingleAllocBuffer(device,
    memory_type,
    0,
    aabbBuffersize,
    bufferUsages | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
    queueFamilyIndex);
  if(!aabbBuffer.Get()){
    KRUST_LOG_ERROR << "Failed to create a new buffer to generate AABBs into in function \"" << __FUNCTION__ << "\"." << kr::endlog;
  } else {
    // Build all resources required to run the compute shader:

    // Load the spir-v shader code into a module:
    kr::ShaderBuffer spirv { kr::loadSpirV(SPHERE_TO_AABB_SHADER_NAME) };
    if(spirv.empty()){
      return nullptr;
    }

    auto shaderModule = kr::ShaderModule::New(device, 0, spirv);

    const auto ssci = kr::PipelineShaderStageCreateInfo(
      0,
      VK_SHADER_STAGE_COMPUTE_BIT,
      *shaderModule,
      "main",
      nullptr // VkSpecializationInfo
    );

    // Define the descriptor and pipeline layouts:

    const VkDescriptorSetLayoutBinding bindings[] {
      kr::DescriptorSetLayoutBinding(
        0, // Binding to the first location
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr // No immutable samplers.
      ),

      kr::DescriptorSetLayoutBinding(
        1, // Binding to the second location
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        nullptr // No immutable samplers.
      )
    };

    auto descriptorSetLayout = kr::DescriptorSetLayout::New(device, 0, 2, bindings);
    auto pipelineLayout = kr::PipelineLayout::New(device,
      0,
      *descriptorSetLayout
    );

    // Construct our compute pipeline:
    auto computePipeline = kr::ComputePipeline::New(device, kr::ComputePipelineCreateInfo(
      0,    // no flags
      ssci,
      *pipelineLayout,
      VK_NULL_HANDLE, // No base pipeline
      -1              // No index of a base pipeline.
    ));

    auto descriptorPool = kr::DescriptorPool::New(device, 0, 2, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2});
    auto set = kr::DescriptorSet::Allocate(*descriptorPool, *descriptorSetLayout);
    VkDescriptorBufferInfo bufferInfos[] = {
      kr::DescriptorBufferInfo(sphereBuffer, 0, VK_WHOLE_SIZE),
      kr::DescriptorBufferInfo(*aabbBuffer, 0, VK_WHOLE_SIZE),
    };
    VkWriteDescriptorSet writes[] = {
      kr::WriteDescriptorSet(*set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfos[0], nullptr),
      kr::WriteDescriptorSet(*set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfos[1], nullptr)
    };
    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    auto commandBuffer = kr::CommandBuffer::New(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto commandBufferInheritanceInfo = kr::CommandBufferInheritanceInfo(nullptr, 0,
      nullptr, VK_FALSE, 0, 0);
    auto bufferBeginInfo = kr::CommandBufferBeginInfo(0, &commandBufferInheritanceInfo);

    VkResult result = vkBeginCommandBuffer(*commandBuffer, &bufferBeginInfo);
    if(VK_SUCCESS != result)
    {
      KRUST_LOG_ERROR << "Failed to begin command buffer in " << __FUNCTION__ << ". Error: " << kr::ResultToString(result) << Krust::endlog;
      return nullptr;
    }

    // Barrier at start to make sure the spheres are ready to be accessed:
    /// @note Design-wise this is commiting special knowledge of what has previously happened to the buffer to fixed code. we shouldn't have to know that a barrier is needed inside this function.
    /*auto barrier = kr::BufferMemoryBarrier2KHR(
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
      queueFamilyIndex, queueFamilyIndex, sphereBuffer, 0, VK_WHOLE_SIZE);*/
    auto barrier = kr::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, queueFamilyIndex, queueFamilyIndex, sphereBuffer, 0, VK_WHOLE_SIZE);
    vkCmdPipelineBarrier(
        *commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, // VkDependencyFlags
        0, nullptr,
        1, &barrier,
        0, nullptr);

    /// bind descriptors
    vkCmdBindDescriptorSets(
      *commandBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE,
      *pipelineLayout,
      0,
      1,
      set->GetHandleAddress(),
      0, nullptr // No dynamic offsets.
      );

    /// Run shader kernel
    vkCmdBindPipeline(*commandBuffer,VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkCmdDispatch(*commandBuffer, numSpheres, 1, 1);

    if(VK_SUCCESS != (result = vkEndCommandBuffer(*commandBuffer)))
    {
      KRUST_LOG_ERROR << "Failed to end command buffer in " << __FUNCTION__ << ". Error: " << kr::ResultToString(result) << Krust::endlog;
      return nullptr;
    }
    const auto submitInfo = kr::SubmitInfo(0, nullptr, nullptr, 1, commandBuffer->GetVkCommandBufferAddress(), 0, nullptr);
    if(VK_SUCCESS != (result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)))
    {
      return nullptr;
    }
    if(VK_SUCCESS != (result = vkQueueWaitIdle(queue))) /// @todo Fixme: an idle queue is the last thing we should be creating. <--------------------------------------[FixMe]
    {
      KRUST_LOG_WARN << "vkQueueWaitIdle() failed  in " << __FUNCTION__ << " but carrying on regardless. Error: " << kr::ResultToString(result) << Krust::endlog;
    }
  }
  return aabbBuffer;
}

/// @todo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BOOKMARK

/**
 * Synchronously build a single BLAS, allocating and cleaning up resources.
 * ## Optimisations:
 * @todo Restructure so that caller generates bboxes directing into host-visible memory to avoid a copy here.
 * @todo Send up 4 float spheres and generate the six aabb components is a CS kernel.
 */
inline void buildSingleGeomAABBBLAS(kr::CommandBufferPtr commandBuffer,
/// @todo Make this a buffer that is already in device memory.
const kr::span<AABBf> abbs,
const uint32_t staging_memory_type, uint32_t device_memory_type, uint32_t queueFamilyIndex)
{
  /// @todo BOOKMARK

  kr::DevicePtr device {commandBuffer->GetDevice()};
  VkDeviceOrHostAddressKHR nullAddress;
  nullAddress.deviceAddress = 0;
 //const auto bboxes_memsize = abbs.size_bytes();
  // Allocate buffers: 1 host-visible for upload of the bboxes, 1 permanent, and one for scratch.
  //auto sphere_staging = createStagingBuffer(*device, staging_memory_type, bb
  //auto staging_memory = kr::DeviceMemory::New(*device, kr::MemoryAllocateInfo(bboxes_memsize, staging_memory_type));

  // Copy the AABBs intothe staging memory/buffer: ---------------- [ToDo]

  VkDeviceOrHostAddressConstKHR aabbsAddress;
  aabbsAddress.deviceAddress = 0u; /// @todo Get the real address of te buffer in memory.
  VkAccelerationStructureGeometryDataKHR geodata;
  geodata.aabbs = kr::AccelerationStructureGeometryAabbsDataKHR(
      aabbsAddress, // VkDeviceOrHostAddressConstKHR /// @todo Address on deviceof the staging buffer.<-------------------------------------------------------------- TODO
      24u // VkDeviceSize                     stride;
    );
  const auto asg = kr::AccelerationStructureGeometryKHR(
    VK_GEOMETRY_TYPE_AABBS_KHR,
    geodata,
    VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR // VkGeometryFlagsKHR                        flags;
  );

  // Work out how big the scratch buffer for building the AS and the final buffer
  // to hold it need to be:
  auto buildInfo = kr::AccelerationStructureBuildGeometryInfoKHR(
    VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
    VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    VK_NULL_HANDLE, // Src acceleration structure.
    VK_NULL_HANDLE, // Src acceleration structure.
    uint32_t(1), // geometryCount,
    &asg, //const VkAccelerationStructureGeometryKHR* pGeometries,
    nullptr, // const VkAccelerationStructureGeometryKHR* const* ppGeometries,
    nullAddress // (Currently 0) VkDeviceOrHostAddressKHR scratchData
  );
  auto buildSizes = kr::AccelerationStructureBuildSizesInfoKHR();
  uint32_t maxPrimitiveCount = abbs.size();
  vkGetAccelerationStructureBuildSizesKHR(*device,
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &buildInfo,
    &maxPrimitiveCount, // Or just make this nullptr?
    &buildSizes
  );

  /*auto asBuffer = kr::Buffer::New(*device, 
  0, // VkBufferCreateFlagBits No special flags needed.
  deviceSize, /// @todo <-------------
  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
  VK_SHARING_MODE_EXCLUSIVE, // VUID-vkQueueSubmit-pSubmits-02808 Any resource created with VK_SHARING_MODE_EXCLUSIVE that is read by an operation specified by pSubmits must not be owned by any queue family other than the one which queue belongs to, at the time it is executed.
  queueFamilyIndex
  );
  // Bind the buffer to some backing memory:
  /// @todo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  auto accelerationStructure = kr::AccelerationStructure::New(*device, 0,
    *asBuffer,
    offset,  /// @todo <-------------
    size,    /// @todo <-------------
    VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    0   // used in trace capture / replay  
  );*/


/*const auto build_geo_info = kr::AccelerationStructureBuildGeometryInfoKHR(
    VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    // Maybe: VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR ||
    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    nullptr,
    accelerationStructure, /// @todo <------------------------------------
    1,
    &asg,
    nullptr,
    scratchDataAddress /// @todo <------------------------------------
  );
  pVkCmdBuildAccelerationStructuresKHR(VK_NULL_HANDLE, 1, &build_geo_info, nullptr);*/
}

const Vec4InMemory g_spheres[] = {
    {0.f, -1000.f, 0.f, 1000.f},
    {-10.294082421925516257f, 0.0903856649139243018f, -10.643888748685666812f, 0.2000000000000000111f},
    {-10.500533092103310651f, 0.11485664765916681063f, -7.7493430311237281316f, 0.2000000000000000111f},
    {-10.871812361167547678f, 0.13143396798238882184f, -4.3541342573361934143f, 0.2000000000000000111f},
    {-10.901405190828475256f, 0.13881706462530019053f, -1.8830734019839909799f, 0.2000000000000000111f},
    {-10.555176083411259569f, 0.14322081212094417424f, 1.4717749166225759794f, 0.2000000000000000111f},
    {-10.68208156444389445f, 0.13390635628866220941f, 4.2547021001717837407f, 0.2000000000000000111f},
    {-10.826102257327576694f, 0.11637002089139514283f, 7.0768584943706454027f, 0.2000000000000000111f},
    {-10.125382387315093879f, 0.096596921713171468582f, 10.213395964081550815f, 0.2000000000000000111f},
    {-7.5209350810358266415f, 0.11766963970285360119f, -10.398192629350951677f, 0.2000000000000000111f},
    {-7.6809181056562110257f, 0.14295664397138807544f, -7.4235956590781544406f, 0.2000000000000000111f},
    {-7.684381419135823954f, 0.16167968505101271148f, -4.1958040687068400842f, 0.2000000000000000111f},
    {-7.2665082340496827129f, 0.17225705905445920507f, -1.6413614718285305383f, 0.2000000000000000111f},
    {-7.8668495030466161211f, 0.16739143544737089542f, 1.8280558276598279921f, 0.2000000000000000111f},
    {-7.6109418753519824108f, 0.16057037747941649286f, 4.5767921042037684742f, 0.2000000000000000111f},
    {-7.830777097411295351f, 0.14340694117231578275f, 7.2030883761917730013f, 0.2000000000000000111f},
    {-7.3149777231871739858f, 0.11870896278071541019f, 10.445050671500798245f, 0.2000000000000000111f},
    {-4.4736552418584958346f, 0.13179000227000869927f, -10.790228710853980942f, 0.2000000000000000111f},
    {-4.144740420863589847f, 0.1599546551402681871f, -7.9326055663705092869f, 0.2000000000000000111f},
    {-4.4244064949433923317f, 0.18169041469673175015f, -4.1292597844354883563f, 0.2000000000000000111f},
    {-4.2622463976820865739f, 0.18966299651731333142f, -1.5847051961438809453f, 0.2000000000000000111f},
    {-4.1019438655431725849f, 0.19096950749337793241f, 1.1129115799489357475f, 0.2000000000000000111f},
    {-4.8375586636670391272f, 0.17802276027873631392f, 4.5344033251360773562f, 0.2000000000000000111f},
    {-4.4783423052490878291f, 0.16187698767078018136f, 7.4969507599171194556f, 0.2000000000000000111f},
    {-4.2098774837206125454f, 0.13582877753015054623f, 10.518599101364122461f, 0.2000000000000000111f},
    {-1.9680102932504754953f, 0.14364530417572041188f, -10.433393175801032982f, 0.2000000000000000111f},
    {-1.1147343661580904062f, 0.17355871679637857596f, -7.1867802994092606639f, 0.2000000000000000111f},
    {-1.4334504479308602942f, 0.18756658348445398588f, -4.7767009139689280417f, 0.2000000000000000111f},
    {-1.4139774267218228054f, 0.19738318487839023874f, -1.7987045222208548623f, 0.2000000000000000111f},
    {-1.1501116021295367808f, 0.19816350871337817807f, 1.5332831114448832732f, 0.2000000000000000111f},
    {-1.7894744232019763608f, 0.18712080897887517494f, 4.7498578014378312062f, 0.2000000000000000111f},
    {-1.6273722063153812645f, 0.17392387459722158383f, 7.0365944172878860385f, 0.2000000000000000111f},
    {-1.704363777017016357f, 0.14177131484814253781f, 10.657035953480871626f, 0.2000000000000000111f},
    {1.2343727272902853542f, 0.13962017593371456314f, -10.920461451300326416f, 0.2000000000000000111f},
    {1.2598645163759985f, 0.17183241656766767846f, -7.3998906671401831758f, 0.2000000000000000111f},
    {1.1693672189103778702f, 0.1882659825452037694f, -4.701613673739251098f, 0.2000000000000000111f},
    {1.6172022080880978923f, 0.19690506781751082599f, -1.8909653031363977682f, 0.2000000000000000111f},
    {1.2675571765518169887f, 0.19808215577688770281f, 1.4932349814433996116f, 0.2000000000000000111f},
    {1.443140970281525437f, 0.1875461491081296117f, 4.7780615698895161358f, 0.2000000000000000111f},
    {1.542756963181841634f, 0.17343236136002815329f, 7.1249631191184734647f, 0.2000000000000000111f},
    {1.3243293375344302731f, 0.14788559596240702376f, 10.123887086065646912f, 0.2000000000000000111f},
    {4.354604066029644116f, 0.13330823411956771452f, -10.697812117620294714f, 0.2000000000000000111f},
    {4.2592786246861216171f, 0.16150286231709287676f, -7.6724662129299980862f, 0.2000000000000000111f},
    {4.438599178636675191f, 0.18084639884250464092f, -4.3143173534841459116f, 0.2000000000000000111f},
    {4.3549048415925870614f, 0.18973434522229126742f, -1.2530420003091318204f, 0.2000000000000000111f},
    {4.3696646912241083882f, 0.18982504503424024733f, 1.1224557319564687496f, 0.2000000000000000111f},
    {4.6488019306614463133f, 0.17777969358098744124f, 4.7788751634714285998f, 0.2000000000000000111f},
    {4.0894665996306773792f, 0.16655067500698805816f, 7.0842906341590188291f, 0.2000000000000000111f},
    {4.0478427289789680188f, 0.14135876165187255538f, 10.045768444393310403f, 0.2000000000000000111f},
    {7.5349098862233114815f, 0.11626735841241497837f, -10.52220975286187965f, 0.2000000000000000111f},
    {7.536154015252702898f, 0.14391706137269011379f, -7.4425498179483007277f, 0.2000000000000000111f},
    {7.7437713042100648764f, 0.15851950951753224217f, -4.7968592347853862279f, 0.2000000000000000111f},
    {7.7001913613748174114f, 0.16879597448189542774f, -1.7682228234818886392f, 0.2000000000000000111f},
    {7.7037975755516265863f, 0.16904008705762407772f, 1.6070949719251312882f, 0.2000000000000000111f},
    {7.1389979306184185859f, 0.16449130283103841066f, 4.4794023592383309662f, 0.2000000000000000111f},
    {7.4966586266747894385f, 0.14314929412842047896f, 7.5842619586810613441f, 0.2000000000000000111f},
    {7.7588593271807448915f, 0.11501109146013277496f, 10.47877337620339766f, 0.2000000000000000111f},
    {10.136520710384525401f, 0.094676632447658448655f, -10.388874786426175234f, 0.2000000000000000111f},
    {10.51018234599211354f, 0.11762913534016661288f, -7.369122048136542702f, 0.2000000000000000111f},
    {10.599894479440052919f, 0.13282267998067709414f, -4.692465790957628613f, 0.2000000000000000111f},
    {10.607483734091427863f, 0.14296433388983587065f, -1.2538669190695994615f, 0.2000000000000000111f},
    {10.209257856751920102f, 0.14620362641255724157f, 1.839137213240533697f, 0.2000000000000000111f},
    {10.534345281521245496f, 0.13332704743800150027f, 4.7324093770836404005f, 0.2000000000000000111f},
    {10.696951880653317701f, 0.11328906031030783197f, 7.6827251305380714896f, 0.2000000000000000111f},
    {10.596762053918254765f, 0.085718065000719434465f, 10.78448683919089568f, 0.2000000000000000111f},
    {0.f, 1.f, 0.f, 1.f},
    {-4.f, 1.f, 0.f, 1.f},
    {4.f, 1.f, 0.f, 1.f},
};

}

/**
 * Fills its window with a scene generated by ray tracing in a compute shader
 * each frame.
 */
class RayQueries1Application : public Krust::IO::Application
{
  uint32_t DoChooseVulkanVersion() const override
  {
    // Request version 1.2 so we can use later shader features and ray queries:
    return VK_API_VERSION_1_2;
  }

  void DoAddRequiredDeviceExtensions(std::vector<const char*>& extensionNames) const override
  {
    extensionNames.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    extensionNames.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    extensionNames.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
  }

  void DoExtendDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features) override
  {
    mDeviceFeature11.pNext = &mDeviceFeature12;
    mDeviceFeature12.pNext = &mDeviceRayQueryFeatures;
    mDeviceRayQueryFeatures.pNext = &mDeviceAccelerationStructureFeatures;
    mDeviceAccelerationStructureFeatures.pNext = features.pNext;
    features.pNext = &mDeviceFeature11;
  }

  void DoCustomizeDeviceFeatureChain(VkPhysicalDeviceFeatures2 &f2) override
  {
    #define REQUIRE_VK_FEATURE(FEATURE, MSG) \
    KRUST_ASSERT1((FEATURE), (MSG)); \
    if(!(FEATURE)){ \
      KRUST_LOG_ERROR << (MSG); \
    } \
    (FEATURE) = VK_TRUE;

    // Check we have things we do need:
    REQUIRE_VK_FEATURE(f2.features.shaderInt16, "16 bit ints are required in shaders.");
    REQUIRE_VK_FEATURE(mDeviceFeature12.storagePushConstant8, "8 bit ints are required in shader push Constant buffers.");
    REQUIRE_VK_FEATURE(mDeviceFeature12.shaderInt8, "Eight bit integers in shader code required.");
    REQUIRE_VK_FEATURE(mDeviceRayQueryFeatures.rayQuery, "This is a ray query demo so we gotta have the ray query extension.");
    REQUIRE_VK_FEATURE(mDeviceAccelerationStructureFeatures.accelerationStructure, "Ray tracing acceleration structures required.");
    // Need them?
    //VkBool32           accelerationStructureCaptureReplay;
    //VkBool32           accelerationStructureIndirectBuild;
    //VkBool32           accelerationStructureHostCommands;
    //VkBool32           descriptorBindingAccelerationStructureUpdateAfterBind;

    // Turn off things we don't need:
    f2.features.independentBlend = VK_FALSE;
    f2.features.geometryShader = VK_FALSE;
    f2.features.tessellationShader = VK_FALSE;
    f2.features.sampleRateShading = VK_FALSE;
    f2.features.dualSrcBlend = VK_FALSE;
    f2.features.logicOp = VK_FALSE;
    f2.features.multiDrawIndirect = VK_FALSE;
    f2.features.drawIndirectFirstInstance = VK_FALSE;
    f2.features.depthClamp = VK_FALSE;
    f2.features.depthBiasClamp = VK_FALSE;
    f2.features.fillModeNonSolid = VK_FALSE;
    f2.features.depthBounds = VK_FALSE;
    f2.features.wideLines = VK_FALSE;
    f2.features.largePoints = VK_FALSE;
    f2.features.alphaToOne = VK_FALSE;
    f2.features.multiViewport = VK_FALSE;
    f2.features.occlusionQueryPrecise = VK_FALSE;
    f2.features.shaderClipDistance = VK_FALSE;
    f2.features.shaderCullDistance = VK_FALSE;
    f2.features.shaderResourceResidency = VK_FALSE;
    f2.features.shaderResourceMinLod = VK_FALSE;
    // Might want these turned back on at some point:
    f2.features.sparseBinding = VK_FALSE;
    f2.features.sparseResidencyBuffer = VK_FALSE;
    f2.features.sparseResidencyImage2D = VK_FALSE;
    f2.features.sparseResidencyImage3D = VK_FALSE;
    f2.features.sparseResidency2Samples = VK_FALSE;
    f2.features.sparseResidency4Samples = VK_FALSE;
    f2.features.sparseResidency8Samples = VK_FALSE;
    f2.features.sparseResidency16Samples = VK_FALSE;
    f2.features.sparseResidencyAliased = VK_FALSE;
    f2.features.variableMultisampleRate = VK_FALSE;
    f2.features.inheritedQueries = VK_FALSE;
  }


public:
  /**
   * Called by the default initialization once Krust is initialised and a window
   * has been created. Now is the time to do any additional setup.
   */
  virtual bool DoPostInit()
  {
    KRUST_LOG_DEBUG << "DoPostInit() entered." << Krust::endlog;

    #define KRUST_VALIDATE_EXT_PTR(extension) (0 == (vk##extension))

    if( KRUST_VALIDATE_EXT_PTR(CmdBuildAccelerationStructuresKHR) ||
        KRUST_VALIDATE_EXT_PTR(CreateAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(DestroyAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdBuildAccelerationStructuresKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdBuildAccelerationStructuresIndirectKHR) ||
        KRUST_VALIDATE_EXT_PTR(BuildAccelerationStructuresKHR) ||
        KRUST_VALIDATE_EXT_PTR(CopyAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(CopyAccelerationStructureToMemoryKHR) ||
        KRUST_VALIDATE_EXT_PTR(CopyMemoryToAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(WriteAccelerationStructuresPropertiesKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdCopyAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdCopyAccelerationStructureToMemoryKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdCopyMemoryToAccelerationStructureKHR) ||
        KRUST_VALIDATE_EXT_PTR(GetAccelerationStructureDeviceAddressKHR) ||
        KRUST_VALIDATE_EXT_PTR(CmdWriteAccelerationStructuresPropertiesKHR) ||
        KRUST_VALIDATE_EXT_PTR(GetDeviceAccelerationStructureCompatibilityKHR) ||
        KRUST_VALIDATE_EXT_PTR(GetAccelerationStructureBuildSizesKHR)
      )
    {
      KRUST_LOG_ERROR << "Some required extension functions are null.";
      return false;
    }

    BuildFences(*mGpuInterface, VK_FENCE_CREATE_SIGNALED_BIT, mSwapChainImageViews.size(), mSwapChainFences);

    // Allocate a command buffer per swapchain entry:
    KRUST_ASSERT1(mCommandBuffers.size() == 0, "Double init of command buffers.");
    kr::CommandBuffer::Allocate(*mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, unsigned(mSwapChainImageViews.size()), mCommandBuffers);

    // Build all resources required to run the compute shader:

    // Load the spir-v shader code into a module:
    kr::ShaderBuffer spirv { kr::loadSpirV(mShaderName) };
    if(spirv.empty()){
      return false;
    }
    auto shaderModule = kr::ShaderModule::New(*mGpuInterface, 0, spirv);

    const auto ssci = kr::PipelineShaderStageCreateInfo(
      0,
      VK_SHADER_STAGE_COMPUTE_BIT,
      *shaderModule,
      "main",
      nullptr // VkSpecializationInfo
    );

    // Define the descriptor and pipeline layouts:

    const auto fbBinding = kr::DescriptorSetLayoutBinding(
      0, // Binding to the first location
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      1,
      VK_SHADER_STAGE_COMPUTE_BIT,
      nullptr // No immutable samplers.
    );

    auto descriptorSetLayout = kr::DescriptorSetLayout::New(*mGpuInterface, 0, 1, &fbBinding);
    mPipelineLayout = kr::PipelineLayout::New(*mGpuInterface,
      0,
      *descriptorSetLayout,
      kr::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Pushed))
      );

    // Construct our compute pipeline:
    mComputePipeline = kr::ComputePipeline::New(*mGpuInterface, kr::ComputePipelineCreateInfo(
      0,    // no flags
      ssci,
      *mPipelineLayout,
      VK_NULL_HANDLE, // no base pipeline
      -1 // No index of a base pipeline.
    ));

    // Create a descriptor pool and allocate a matching descriptorSet for each image:
    VkDescriptorPoolSize poolSizes[1] = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uint32_t(mSwapChainImages.size())}};
    mDescriptorPool = kr::DescriptorPool::New(*mGpuInterface, 0, uint32_t(mSwapChainImages.size()), uint32_t(1), poolSizes);
    for(unsigned i = 0; i < mSwapChainImages.size(); ++i)
    {
      auto set = kr::DescriptorSet::Allocate(*mDescriptorPool, *descriptorSetLayout);
      mDescriptorSets.push_back(set);
      auto imageInfo = kr::DescriptorImageInfo(VK_NULL_HANDLE, mSwapChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL);
      auto write = kr::WriteDescriptorSet(*set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo, nullptr, nullptr);
      vkUpdateDescriptorSets(*mGpuInterface, 1, &write, 0, nullptr);
    }

    mLinePrinter = std::make_unique<kr::LinePrinter>(*mGpuInterface, kr::span {mSwapChainImageViews});

    // Examine heaps and decide on staging and on-device storage types and heaps:
    const auto staging_mem_type = kr::FindFirstMemoryTypeWithProperties(mGpuMemoryProperties, 0xffffffff, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if(!staging_mem_type){
      KRUST_LOG_ERROR << "No memory type suitable for staging host->device transfers found." << kr::endlog;
      return false;
    }
    const auto device_mem_type = kr::FindMemoryTypeWithAndWithout(mGpuMemoryProperties, 0xffffffff, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if(!device_mem_type){
      KRUST_LOG_ERROR << "No memory type suitable for device-only buffers found." << kr::endlog;
      return false;
    }


    // Build the acceleration structures for the scene:
    auto spheresSpan = kr::span<const Vec4InMemory, kr::dynamic_extent>(g_spheres);
    auto aabbs = spheresToAABBs(spheresSpan);  ///< @todo FixMe. TEMP: we can convert on the GPU in the future when running a compute pass is async.
    auto sphereBuffer = uploadToDeviceBuffer<Vec4InMemory>(
      *mGpuInterface,
      *mDefaultQueue,
      *mCommandPool,
      spheresSpan,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // So we can download it again for confirmation that it uploaded correctly. Hopefully not an anti-optimisation trigger.
      staging_mem_type,
      device_mem_type,
      mDefaultDrawingQueueFamily); ///< @todo Should be on a transfer queue (queue with transfer flag set and fewest other flags also set).

    std::vector<Vec4InMemory> roundtriped_spheres;
    roundtriped_spheres.resize(spheresSpan.size());
    auto downloadedSpheresSpan = kr::span<Vec4InMemory, kr::dynamic_extent>(roundtriped_spheres);
    const bool spheresDownloaded = downloadFromDeviceBuffer(
      *mGpuInterface,
      *sphereBuffer,
      *mDefaultQueue,
      *mCommandPool,
      downloadedSpheresSpan,
      staging_mem_type,
      mDefaultDrawingQueueFamily
    );
    KRUST_ASSERT1(spheresDownloaded, "Spheres couldn't be downloaded to host");
    for(unsigned i = 0; i < spheresSpan.size(); ++i)
    {
      const auto& a = g_spheres[i];
      const auto& b = roundtriped_spheres[i];
      KRUST_ASSERT2(all_of(load(a) == load(b)), "Failed to roundtrip a sphere to device memory and back.");
    }

    kr::BufferPtr aabbBuffer = spheresToAABBs(
      *mDefaultQueue,
      *mCommandPool,
      spheresSpan.size(),
      *sphereBuffer,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // Only needed while debugging so we can download it and compare to a CPU reference.
      device_mem_type,
      mDefaultDrawingQueueFamily
    );

    // Download the buffer full of AABBs to be sure it matches the ones generated on the CPU:
    std::vector<float> downloaded_aabbs;
    downloaded_aabbs.resize(spheresSpan.size() * 6);
    const bool aabbsDownloaded = downloadFromDeviceBuffer(
      *mGpuInterface,
      *aabbBuffer,
      *mDefaultQueue,
      *mCommandPool,
      kr::span<float, kr::dynamic_extent>(downloaded_aabbs),
      staging_mem_type,
      mDefaultDrawingQueueFamily
    );
    KRUST_ASSERT1(aabbsDownloaded, "Downloading the GPU-generated AABBs failed.");
    KRUST_ASSERT1(std::equal(&aabbs[0].minX, &aabbs[spheresSpan.size()].minX, &downloaded_aabbs[0], &downloaded_aabbs[spheresSpan.size()*6]), "The AABBs generated on the GPU differ from the reference made on the CPU.");

    //
    // For aabb bufer: VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    //         Memory must be: VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT

    kr::CommandBufferPtr buildBuffer = kr::CommandBuffer::New(*mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    buildSingleGeomAABBBLAS(buildBuffer, aabbs, staging_mem_type, device_mem_type, mDefaultPresentQueueFamily);




    // BOOKMARK <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    return true;
  }

  bool DoPreDeInit()
  {
    mComputePipeline.Reset();
    mPipelineLayout.Reset();;
    mDescriptorSets.clear();
    mDescriptorPool.Reset();
    mLinePrinter.reset();

    return true;
  }

  virtual void OnKey(bool up, Krust::IO::KeyCode keycode) override
  {
    // 25, 39, 38, 40, 24, 26, // WSADQE
    // 111, 116, 113, 114, 112, 117 // up,down,left,right arrows, pgup, pgdn
    switch(keycode)
    {
      case 25:
      case 111:{
        KRUST_LOG_DEBUG << "Forward key " << (up ? "up" : "down") << kr::endlog;
        mKeyFwd = !up;
        break;
      }
      case 39:
      case 116:{
        KRUST_LOG_DEBUG << "Backwards key " << (up ? "up" : "down") << kr::endlog;
        mKeyBack = !up;
        break;
      }
      case 38:
      case 113:{
        KRUST_LOG_DEBUG << "Left key " << (up ? "up" : "down") << kr::endlog;
        mKeyLeft = !up;
        break;
      }
      case 40:
      case 114:{
        KRUST_LOG_DEBUG << "Right key " << (up ? "up" : "down") << kr::endlog;
        mRightKey = !up;
        break;
      }
      case 26:
      case 112:{
        KRUST_LOG_DEBUG << "Up key " << (up ? "up" : "down") << kr::endlog;
        mKeyUp = !up;
        break;
      }
      case 24:
      case 117:{
        KRUST_LOG_DEBUG << "Down key " << (up ? "up" : "down") << kr::endlog;
        mKeyDown = !up;
        break;
      }
      default:
        KRUST_LOG_WARN << "Unknown key with scancode " << unsigned(keycode) << (up ? " up" : " down") << kr::endlog;
    }
  }

  void OnMouseMove(const Krust::IO::InputTimestamp when, const int x, const int y, const unsigned state) override
  {
    /// @todo Time.
    if(kr::IO::left(state)){
      if(!mLeftMouse){
        mLeftMouse = true;
        mLastX = x;
        mLastY = y;
      }
      float yawRads   = (x - mLastX) * 0.005f;
      float pitchRads = (y - mLastY) * 0.005f;
      /// @todo clamp range of pitch.
      mCameraYaw   += yawRads;
      mCameraPitch -= pitchRads;
      mCameraPitch = std::min(1.55f, std::max(mCameraPitch, -1.55f));
      while(mCameraYaw < 0.0f){
        mCameraYaw += float(3.14159265358979323846 * 2);
      }
      while(mCameraYaw > float(3.14159265358979323846 * 2)){
        mCameraYaw -= float(3.14159265358979323846 * 2);
      }
    } else {
      mLeftMouse = false;
    }
    mLastX = x;
    mLastY = y;
  }

  /**
   * Called by the default run loop to repaint the screen.
   */
  virtual void DoDrawFrame()
  {
    auto start = std::chrono::high_resolution_clock::now();

    static unsigned frameNumber = -1;
    ++frameNumber;
    // KRUST_LOG_INFO << "   ------------ Ray Tracing Example 1: draw frame! frame: " << frameNumber << ". currImage: " << mCurrentTargetImage << ". handle: " << mSwapChainImages[mCurrentTargetImage] << "  ------------\n";

    auto submitFence = mSwapChainFences[mCurrentTargetImage];
    const VkResult fenceWaitResult = vkWaitForFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress(), true, 1000000000); // Wait one second.
    if(VK_SUCCESS != fenceWaitResult)
    {
      KRUST_LOG_ERROR << "Wait for queue submit of main commandbuffer did not succeed: " << fenceWaitResult << Krust::endlog;
    }
    vkResetFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress());

    constexpr VkPipelineStageFlags pipelineFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    auto submitInfo = kr::SubmitInfo(
      1, mSwapChainSemaphore->GetVkSemaphoreAddress(),
      &pipelineFlags,
      // We have one command buffer per presentable image, so submit the right one:
      1, mCommandBuffers[mCurrentTargetImage]->GetVkCommandBufferAddress(),
      0, nullptr);

    // Build a command buffer for the current swapchain entry:

    kr::CommandBufferPtr commandBuffer = mCommandBuffers[mCurrentTargetImage];
    VkImage framebufferImage = mSwapChainImages[mCurrentTargetImage];

    // Empty the command buffer and begin it again from scratch:

    const VkResult resetBufferResult = vkResetCommandBuffer(*commandBuffer, 0);
    if(VK_SUCCESS != resetBufferResult)
    {
      KRUST_LOG_ERROR << "Failed to reset command buffer. Error: " << resetBufferResult << Krust::endlog;
      return;
    }

    auto commandBufferInheritanceInfo = kr::CommandBufferInheritanceInfo(nullptr, 0,
      nullptr, VK_FALSE, 0, 0);
    auto bufferBeginInfo = kr::CommandBufferBeginInfo(0, &commandBufferInheritanceInfo);

    const VkResult beginBufferResult = vkBeginCommandBuffer(*commandBuffer,&bufferBeginInfo);
    if(VK_SUCCESS != beginBufferResult)
    {
      KRUST_LOG_ERROR << "Failed to begin command buffer. Error: " << beginBufferResult << Krust::endlog;
      return;
    }

    // Assume the image is returned from being presented and fix it up using
    // an image memory barrier:
    auto postPresentImageMemoryBarrier = kr::ImageMemoryBarrier( 0,
      VK_ACCESS_SHADER_READ_BIT |
      VK_ACCESS_SHADER_WRITE_BIT |
      VK_ACCESS_MEMORY_READ_BIT |
      VK_ACCESS_MEMORY_WRITE_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      framebufferImage,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    vkCmdPipelineBarrier(
        *commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, // VkDependencyFlags
        0, nullptr,
        0, nullptr,
        1, &postPresentImageMemoryBarrier);

    // Bind the Descriptor set to the current command buffer:
    vkCmdBindDescriptorSets(
      *commandBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE,
      *mPipelineLayout,
      0,
      1,
      mDescriptorSets[mCurrentTargetImage]->GetHandleAddress(),
      0, nullptr // No dynamic offsets.
      );

    // Here is where we kick off our compute shader.

    vkCmdBindPipeline(*commandBuffer,VK_PIPELINE_BIND_POINT_COMPUTE, *mComputePipeline);
    const unsigned win_width  { mWindow->GetPlatformWindow().GetWidth()};
    const unsigned win_height { mWindow->GetPlatformWindow().GetHeight()};

    mPushed.fb_width  = win_width;
    mPushed.fb_height = win_height;
    mPushed.frame_no = frameNumber;
    const float MOVE_SCALE = mMoveScale;

    if(mKeyLeft) {
      kr::store(kr::load(mPushed.ray_origin) + (-kr::load(mPushed.ray_target_right) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mRightKey) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_right) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mKeyFwd) {
      kr::store(kr::load(mPushed.ray_origin) + kr::cross(kr::load(mPushed.ray_target_up), kr::load(mPushed.ray_target_right)) * MOVE_SCALE, mPushed.ray_origin);
    }
    if(mKeyBack) {
      kr::store(kr::load(mPushed.ray_origin) + kr::cross(kr::load(mPushed.ray_target_right), kr::load(mPushed.ray_target_up)) * MOVE_SCALE, mPushed.ray_origin);
    }
    if(mKeyUp) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_up) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mKeyDown) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_up) * -MOVE_SCALE) , mPushed.ray_origin);
    }
    mPushed.ray_origin[1] = kr::clamp(mPushed.ray_origin[1], -30.0f, 1500.0f);

    // Work out camera direction from mouse-defined angles:
    kr::Vec3 right, up, fwd;
    viewVecsFromAngles(mCameraPitch, mCameraYaw, right, up, fwd);
    auto ray_origin = kr::make_vec3(mPushed.ray_origin);
    // The ray target origin is the bottom-left corner of the worldspace 2d grid,
    // equivalent to the pixels of the framebuffer, that we will shoot rays at.
    auto ray_target_origin = ray_origin
        //+ fwd * (win_width * 0.5f)
        + fwd * (win_height * 0.5f)
        + (-right) * (win_width * 0.5f)
        + (-up)    * (win_height * 0.5f);

    kr::store(ray_target_origin, mPushed.ray_target_origin);
    kr::store(right,             mPushed.ray_target_right);
    kr::store(up,                mPushed.ray_target_up);


    vkCmdPushConstants(*commandBuffer, *mPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Pushed), &mPushed);

    vkCmdDispatch(*commandBuffer,
      win_width / WORKGROUP_X + (win_width % WORKGROUP_X ? 1 : 0 ),
      win_height / WORKGROUP_Y + (win_width % WORKGROUP_Y ? 1 : 0 ),
      1);

    /// @todo Memory barrier here waiting for writes to the framebuffer from the main kernel to complete.

    mLinePrinter->SetFramebuffer(mSwapChainImageViews[mCurrentTargetImage], mCurrentTargetImage);
    mLinePrinter->BindCommandBuffer(*commandBuffer, mCurrentTargetImage);

    std::chrono::duration<double> diff = start - mFrameInstant;
    const float fps = 1.0f / diff.count();
    mAmortisedFPS = (mAmortisedFPS * 7 + fps) * 0.125f;
    char buffer[126];
    snprintf(buffer, sizeof(buffer)-1, "FPS: %.1f", mAmortisedFPS);
    mLinePrinter->PrintLine(*commandBuffer, 0, 0, 3, 0, true, true, buffer);
    snprintf(buffer, sizeof(buffer)-1, "MS: %.2f", float(diff.count() * 1000));
    mLinePrinter->PrintLine(*commandBuffer, 0, 1, 3, 0, true, true, buffer);
    strcpy(buffer, "GPU: ");
    std::copy(&mGpuProperties.deviceName[0], &(mGpuProperties.deviceName[120]), &buffer[5]);
    buffer[125] = 0;
    mLinePrinter->PrintLine(*commandBuffer, 0, 2, 2, 0, true, true, buffer);

    // Assume the framebuffer will be presented so insert an image memory
    // barrier here first:
    auto presentBarrier = kr::ImageMemoryBarrier(
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      framebufferImage,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &presentBarrier);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(*commandBuffer);
    if(endCommandBufferResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to end command buffer with result: " << endCommandBufferResult << Krust::endlog;
      return;
    }

    // Execute command buffer on main queue:
    // KRUST_LOG_DEBUG << "Submitting command buffer " << mCurrentTargetImage << "(" << *(mCommandBuffers[mCurrentTargetImage]) << ")." << Krust::endlog;
    const VkResult submitResult = vkQueueSubmit(*mDefaultGraphicsQueue, 1, &submitInfo, *submitFence);
    if(submitResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to submit command buffer. Result: " << submitResult << Krust::endlog;
      return;
    }

    mFrameInstant = start;
  }

  ~RayQueries1Application()
  {
    KRUST_LOG_DEBUG << "RayQueries1Application::~RayQueries1Application()" << Krust::endlog;
  }

private:
  // Data:
  VkPhysicalDeviceVulkan11Features    mDeviceFeature11 = kr::PhysicalDeviceVulkan11Features();
  VkPhysicalDeviceVulkan12Features    mDeviceFeature12 = kr::PhysicalDeviceVulkan12Features();
  VkPhysicalDeviceRayQueryFeaturesKHR mDeviceRayQueryFeatures = kr::PhysicalDeviceRayQueryFeaturesKHR();
  VkPhysicalDeviceAccelerationStructureFeaturesKHR mDeviceAccelerationStructureFeatures = kr::PhysicalDeviceAccelerationStructureFeaturesKHR();
  // Functions from required extensions:
  PFN_vkCmdBuildAccelerationStructuresKHR mCmdBuildAccelerationStructuresKHR = nullptr;
  kr::PipelineLayoutPtr mPipelineLayout;
  kr::DescriptorPoolPtr mDescriptorPool;
  /// One descriptor set per swapchain image.
  std::vector<kr::DescriptorSetPtr> mDescriptorSets;
  kr::ComputePipelinePtr mComputePipeline;
  std::unique_ptr<kr::LinePrinter> mLinePrinter;
  std::chrono::time_point<std::chrono::high_resolution_clock> mFrameInstant = std::chrono::high_resolution_clock::now();
  float mAmortisedFPS = 30;

  // Camera euler angles in radians. The mouse will drive these.
  float mCameraPitch = 0;
  float mCameraYaw = 0;
  // Keys currently depressed.
  bool mKeyLeft     = false;
  bool mRightKey    = false;
  bool mKeyFwd      = false;
  bool mKeyBack     = false;
  bool mKeyUp    = false;
  bool mKeyDown  = false;
  // Mouse state:
  bool mLeftMouse = false;
  int mLastX = 0;
  int mLastY = 0;
public:
  // Push Constants for first 2 shaders over checkerboard.
  Pushed mPushed1 = {
    1, // width
    1, // height
    0, //frame number
    0.0f, // padding
    {0,405,900}, 1,
    {-900,0,0}, 1,
    {1,0,0}, 1,
    {0,1,0}, 1,
  };
  // Push Constants for third shader based on RTIOW.
  Pushed mPushed = {
    1, // width
    1, // height
    0, //frame number
    0.0f, // padding
    {0,0,0 + 10}, 1, // ray_origin
    {-900,-405,-900 + 10}, 1,  // ray_target_origin
    {1,0,0}, 1,     //  ray_target_right
    {0,1,0}, 1,     // ray_target_up
  };
  float mMoveScale = 0.0625f;
  std::unordered_map<std::string, ShaderParams> mShaderParamsOptions {
    {RT1_SHADER,  {mPushed1, 7.5f}},
    {RT2_SHADER,  {mPushed1, 6.5f}},
    {GREY_SHADER, {mPushed,  0.0625f}},
    {MATERIALS_SHADER, {mPushed,  0.0625f}},
  };
  ShaderParams* mShaderParams {&mShaderParamsOptions[MATERIALS_SHADER]};
  const char* mShaderName = MATERIALS_SHADER;
};


int main(const int argc, const char* argv[])
{
  puts("Ray Tracing in a GPU compute shader using ray queries.\n"
      "Usage:\n"
      "1. rt1\n"
      "2. rt1 compiled_spirv_shader_filename\n"
      "3. rt1 compiled_spirv_shader_filename true # To disable vsync\n"
  );

  RayQueries1Application application;
  application.SetName("Ray Queries 1");
  application.SetVersion(1);
  uint8_t keycodes[] = {
    25, 39, 38, 40, 24, 26, // WSADQE
    111, 116, 113, 114, 112, 117 // up,down,left,right arrows, pgup, pgdn
  };
  application.ListenToScancodes(keycodes, sizeof(keycodes));
  if(argc > 1){
    application.mShaderName = argv[1];
    const ShaderParams& params = application.mShaderParamsOptions[application.mShaderName];
    application.mPushed = params.push_defaults;
    application.mMoveScale = params.move_scale;
  }
  bool allowTearing = false;
  if(argc > 2){
    const char * tr = "true";
    if(!strcasecmp(tr, argv[2])){
      allowTearing = true;
    }
  }

  // Request a busy loop which constantly repaints to show the
  // animation:
  const int status = application.Run(Krust::IO::MainLoopType::Busy, VK_IMAGE_USAGE_STORAGE_BIT, allowTearing);

  KRUST_LOG_INFO << "Exiting cleanly with code " << status << ".\n";
  return status;
}
