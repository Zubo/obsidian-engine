#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace obsidian::vk_rhi::vkinit {

VkCommandPoolCreateInfo
commandPoolCreateInfo(std::uint32_t queueFamilyIndex,
                      VkCommandPoolCreateFlags flags = 0);

VkCommandBufferAllocateInfo commandBufferAllocateInfo(
    VkCommandPool pool, std::uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits shaderStageFlags,
                              VkShaderModule shaderModule);

VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo
inputAssemblyCreateInfo(VkPrimitiveTopology primitiveTopology);

VkPipelineRasterizationStateCreateInfo
rasterizationCreateInfo(VkPolygonMode polygonMode,
                        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);

VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();

VkPipelineColorBlendAttachmentState colorBlendAttachmentState();

VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags);

VkImageCreateInfo imageCreateInfo(VkImageUsageFlags usageFlags,
                                  VkExtent3D extent, VkFormat format);

VkImageViewCreateInfo imageViewCreateInfo(VkImage image, VkFormat format,
                                          VkImageAspectFlags imageAspectFlags);

VkPipelineDepthStencilStateCreateInfo
depthStencilStateCreateInfo(bool depthTestEnable);

VkCommandBufferBeginInfo commandBufferBeginInfo(
    VkCommandBufferUsageFlags flags,
    VkCommandBufferInheritanceInfo const* inheritanceInfo = nullptr);

VkSubmitInfo commandBufferSubmitInfo(VkCommandBuffer const* cmd);

VkSamplerCreateInfo
samplerCreateInfo(VkFilter filter, VkSamplerMipmapMode mipmapMode,
                  VkSamplerAddressMode addressMode,
                  std::optional<float> maxAnisotropy = std::nullopt);

VkImageMemoryBarrier layoutImageBarrier(VkImage image, VkImageLayout oldLayout,
                                        VkImageLayout newLayout,
                                        VkImageAspectFlagBits aspectMask);

} /*namespace obsidian::vk_rhi::vkinit*/
