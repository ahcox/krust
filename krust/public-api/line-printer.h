#ifndef KRUST_COMMON_PUBLIC_API_LINE_PRINTER_H
#define KRUST_COMMON_PUBLIC_API_LINE_PRINTER_H

// Copyright (c) 2021 Andrew Helge Cox
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
 * @file Printing a line of text into an image buffer using a compute shader
 * using an 8*16 bitmap font on a character grid.
 */

// Internal includes:
#include "krust/public-api/vulkan-objects.h"
#include "krust/public-api/vulkan-utils.h"
#include "krust-kernel/public-api/span.h"

// External includes:
#include <vulkan/vulkan.h>
#include <vector>

namespace Krust
{

/// Push constants used to send the string to print.
struct LinePrinterFrameParams
{
    uint8_t fb_char_x;
    uint8_t fb_char_y;
    uint8_t fg_bg_colours; // 3 bit palleted foreground and background colour stuffed into 2 nibbles.
    uint8_t str[125];
};

/**
 * @brief Manages and dispatches a shader to draw text with a bitmap font.
 *
 * You might want to issue an image memory barrier to allow all preceding writes
 * to complete before doing your first text print over an existing rendering.
 * There's no need to have barriers between each line unless you are overwriting
 * text on text.
 */
class LinePrinter
{
public:
    /// @param image_views The images that can be printed into (e.g. all the images in a swapchain).
    LinePrinter(Krust::Device& device, span<VkImageView> image_views) : mDevice(&device)
    {
        // Build all resources required to run the compute shader:

        // Load the spir-v shader code into a module:
        ShaderBuffer spirv { loadSpirV(mShaderName) };
        if(spirv.empty()){
            ThreadBase::Get().GetErrorPolicy().Error(Errors::IllegalState, "Failed to load SPIR-V shader.", __FUNCTION__, __FILE__, __LINE__);
        }
        auto shaderModule = ShaderModule::New(device, 0, spirv);

        const auto ssci = PipelineShaderStageCreateInfo(
            0,
            VK_SHADER_STAGE_COMPUTE_BIT,
            *shaderModule,
            "main",
            nullptr // VkSpecializationInfo
        );

        // Define the descriptor and pipeline layouts:

        const auto fbBinding = DescriptorSetLayoutBinding(
            0, // Binding to the first location
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr // No immutable samplers.
        );

        mDescriptorSetLayout = DescriptorSetLayout::New(device, 0, 1, &fbBinding);
        mPipelineLayout = PipelineLayout::New(device,
            0,
            *mDescriptorSetLayout,
            PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LinePrinterFrameParams))
        );

        // Construct our compute pipeline:
        mComputePipeline = ComputePipeline::New(device, ComputePipelineCreateInfo(
            0,    // no flags
            ssci,
            *mPipelineLayout,
            VK_NULL_HANDLE, // no base pipeline
            -1 // No index of a base pipeline.
        ));

        // Create a descriptor pool and allocate a matching descriptorSet for each image
        // we are asked to render into:
        VkDescriptorPoolSize poolSizes[1] = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u}};
        mDescriptorPool = DescriptorPool::New(device, 0, image_views.size(), uint32_t(1), poolSizes);
        unsigned slot = 0;
        for(auto& view : image_views){
            mDescriptorSets.push_back( DescriptorSet::Allocate(*mDescriptorPool, *mDescriptorSetLayout));
            SetFramebuffer(view, slot++);
        }
    }

    /// Once per frame call this to make sure we write text into the correct image.
    void SetFramebuffer(VkImageView imageView, unsigned slot)
    {
        auto imageInfo  = DescriptorImageInfo(VK_NULL_HANDLE, imageView, VK_IMAGE_LAYOUT_GENERAL);
        auto write = WriteDescriptorSet(*mDescriptorSets[slot % mDescriptorSets.size()], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo, nullptr, nullptr);
        vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
    }

    /// Bind the descriptor set to the command buffer and other ops done before a
    /// sequence of prints.
    void BindCommandBuffer(VkCommandBuffer commandBuffer, unsigned slot)
    {
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            *mPipelineLayout,
            0,
            1,
            mDescriptorSets[slot % mDescriptorSets.size()]->GetHandleAddress(),
            0, nullptr // No dynamic offsets.
        );
        vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_COMPUTE, *mComputePipeline);
    }

    /// Print a line of 125 8*16 pixel chars at a top-left relative location
    /// specified in multiples of a whole 8*16 char.
    void PrintLine(
        VkCommandBuffer commandBuffer,
        const uint8_t x, const uint8_t y,
        // Colours from 8 colour palette.
        const uint8_t foreground, const uint8_t background,
        bool semitransparent,
        bool show_background,
        // Between 0 and 125 characters to print.
        const std::string_view& msg)
    {
        const size_t msg_len = std::min(msg.size(), size_t(125));
        params.fb_char_x = x;
        params.fb_char_y = y;
        params.fg_bg_colours = (foreground << 5u) + (background << 2u) + (uint8_t(semitransparent) << 1) + uint8_t(show_background);
        std::copy(&msg[0], &msg[msg_len], &params.str[0]);
        vkCmdPushConstants(commandBuffer, *mPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LinePrinterFrameParams), &params);
        // One workgroup per character will yield one invocation per pixel in the message:
        vkCmdDispatch(commandBuffer, msg_len, 1, 1);
    }
private:
    const char* mShaderName {"text_print.comp.spv"};
    Krust::DevicePtr mDevice;
    DescriptorSetLayoutPtr mDescriptorSetLayout;
    PipelineLayoutPtr mPipelineLayout;
    ComputePipelinePtr mComputePipeline;
    DescriptorPoolPtr mDescriptorPool;
    /// We need one of these per image in the app's present queue unless we wait for
    /// the GPU every frame.
    std::vector<DescriptorSetPtr> mDescriptorSets;
    LinePrinterFrameParams params;
};


} /* namespace Krust */

#endif // KRUST_COMMON_PUBLIC_API_LINE_PRINTER_H
