#pragma once

#include <obsidian/vk_rhi/vk_types.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <optional>

namespace obsidian::vk_rhi {

struct Framebuffer {
  VkFramebuffer vkFramebuffer = VK_NULL_HANDLE;
  AllocatedImage colorBufferImage = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  VkImageView colorBufferImageView = VK_NULL_HANDLE;
  AllocatedImage depthBufferImage = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  VkImageView depthBufferImageView = VK_NULL_HANDLE;
};

struct AttachmentImageUsage {
  VkImageUsageFlags colorImageUsage = 0;
  VkImageUsageFlags depthImageUsage = 0;
};

struct RenderPass {
  VkDevice vkDevice;
  VkRenderPass vkRenderPass;
  std::optional<VkFormat> colorAttachmentFormat;
  std::optional<VkFormat> depthAttachmentFormat;

  // generates the images
  Framebuffer
  generateFramebuffer(VmaAllocator vmaAllocator, VkExtent2D extent,
                      AttachmentImageUsage attachmentUsages,
                      VkImageView overrideColorImageView = VK_NULL_HANDLE);
};

} /*namespace obsidian::vk_rhi*/
