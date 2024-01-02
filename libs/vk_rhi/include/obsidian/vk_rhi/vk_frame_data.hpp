#pragma once

#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <vulkan/vulkan.h>

#include <array>

namespace obsidian::vk_rhi {

struct FrameData {
  VkSemaphore vkRenderSemaphore;
  VkSemaphore vkPresentSemaphore;
  VkFence vkRenderFence;
  VkCommandPool vkCommandPool;
  VkCommandBuffer vkCommandBuffer;

  VkDescriptorSet vkMainRenderPassDescriptorSet;

  std::array<Framebuffer, rhi::maxLightsPerDrawPass> shadowFrameBuffers;

  Framebuffer vkDepthPrepassFramebuffer;

  Framebuffer vkSsaoFramebuffer;
  VkDescriptorSet vkSsaoRenderPassDescriptorSet;

  Framebuffer vkSsaoPostProcessingFramebuffer;
  VkDescriptorSet vkSsaoPostProcessingDescriptorSet;

  PendingResourcesToDestroy pendingResourcesToDestroy;
};

} /*namespace obsidian::vk_rhi*/
