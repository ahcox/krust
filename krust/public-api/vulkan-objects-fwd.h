#ifndef KRUST_VULKAN_OBJECTS_FWD_H_INCLUDED_E26EF
#define KRUST_VULKAN_OBJECTS_FWD_H_INCLUDED_E26EF

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

// Internal includes
#include "krust/public-api/intrusive-pointer.h"

/**
* @file
*
* Forward declarations for Krust implementation of RAII for Vulkan API objects.
*/

namespace Krust
{

/**
 * @defgroup VulkanObjectOwners Vulkan Object Owners
 *
 * Reference counted wrappers for Vulkan objects and smart pointers to them that
 * keep them alive as long as necessary and clean them up when they are no
 * longer in use.
 */
///@{

// -----------------------------------------------------------------------------
class Instance;
/**
* Shared pointer to automatically manage the lifetime of an Instance object and
* thus the underlying Vulkan API object.
*/
using InstancePtr = IntrusivePointer<Instance>;

// -----------------------------------------------------------------------------
class Device;
using DevicePtr = IntrusivePointer<Device>;

// -----------------------------------------------------------------------------
class Buffer;
using BufferPtr = IntrusivePointer<Buffer>;

// -----------------------------------------------------------------------------
class CommandPool;
using CommandPoolPtr = IntrusivePointer<CommandPool>;

// -----------------------------------------------------------------------------
class CommandBuffer;
using CommandBufferPtr = IntrusivePointer<CommandBuffer>;

// -----------------------------------------------------------------------------
class ComputePipeline;
/**
* Shared pointer to automatically manage the lifetime of a ComputePipeline object and
* thus the underlying Vulkan API object.
*/
using ComputePipelinePtr = IntrusivePointer<ComputePipeline>;

// -----------------------------------------------------------------------------
class DescriptorPool;
using DescriptorPoolPtr = IntrusivePointer<DescriptorPool>;

// -----------------------------------------------------------------------------
class DescriptorSetLayout;
using DescriptorSetLayoutPtr = IntrusivePointer<DescriptorSetLayout>;

// -----------------------------------------------------------------------------
class DescriptorSet;
using DescriptorSetPtr = IntrusivePointer<DescriptorSet>;

// -----------------------------------------------------------------------------
class DeviceMemory;
using DeviceMemoryPtr = IntrusivePointer<DeviceMemory>;

// -----------------------------------------------------------------------------
class Fence;
using FencePtr = IntrusivePointer<Fence>;

// -----------------------------------------------------------------------------
class Image;
using ImagePtr = IntrusivePointer<Image>;

// -----------------------------------------------------------------------------
class ImageView;
using ImageViewPtr = IntrusivePointer<ImageView>;

// -----------------------------------------------------------------------------
class PipelineLayout;
using PipelineLayoutPtr = IntrusivePointer<PipelineLayout>;

// -----------------------------------------------------------------------------
class ShaderModule;
using ShaderModulePtr = IntrusivePointer<ShaderModule>;

///@}

}

#endif // #ifndef KRUST_VULKAN_OBJECTS_FWD_H_INCLUDED_E26EF
