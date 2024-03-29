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
                                  VkExtent3D extent, VkFormat format,
                                  std::uint32_t mipLevels = 1,
                                  std::uint32_t arrayLayers = 1);

VkImageViewCreateInfo imageViewCreateInfo(VkImage image, VkFormat format,
                                          VkImageAspectFlags imageAspectFlagsm,
                                          std::uint32_t mipLevels = 1,
                                          std::uint32_t arrayLayers = 1);

VkPipelineDepthStencilStateCreateInfo
depthStencilStateCreateInfo(bool depthTestEnable, bool const depthTestWrite);

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
                                        VkImageAspectFlagBits aspectMask,
                                        std::uint32_t mipLevelCount = 1,
                                        std::uint32_t layerCount = 1);

VkAttachmentDescription colorAttachmentDescription(VkFormat format,
                                                   VkImageLayout finalLayout);

VkAttachmentDescription depthAttachmentDescription(VkFormat format);

} /*namespace obsidian::vk_rhi::vkinit*/
