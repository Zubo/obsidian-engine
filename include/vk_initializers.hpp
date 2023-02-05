#pragma once

#include <vk_types.hpp>

#include <cstdint>

namespace vkinit {

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

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags);

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags);

} // namespace vkinit
