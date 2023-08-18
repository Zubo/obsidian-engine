#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <optional>

namespace obsidian::rhi_vk::vkinit {

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
rasterizationCreateInfo(VkPolygonMode polygonMode);

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

} /*namespace obsidian::rhi_vk::vkinit*/
