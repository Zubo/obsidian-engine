#pragma once

#include <vk_types.hpp>

#include <cstdint>
#include <vulkan/vulkan_core.h>

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

VkImageCreateInfo imageCreateInfo(VkImageCreateFlags flags, VkExtent3D extent,
                                  VkFormat format);

VkImageViewCreateInfo imageViewCreateInfo(VkImage image, VkFormat format,
                                          VkImageAspectFlags imageAspectFlags);

VkPipelineDepthStencilStateCreateInfo
depthStencilStateCreateInfo(bool depthTestEnable);

VkDescriptorSetLayoutBinding
descriptorSetLayoutBinding(std::uint32_t binding,
                           VkDescriptorType descriptorType,
                           VkShaderStageFlags stageFlags);

VkDescriptorSetLayoutCreateInfo
descriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding const* pBindings,
                              std::uint32_t bindingCount);

VkDescriptorSetAllocateInfo
descriptorSetAllocateInfo(VkDescriptorPool descriptorPool,
                          VkDescriptorSetLayout const* descriptorSetLayouts,
                          std::uint32_t descriptorSetLayoutCount);

VkWriteDescriptorSet
writeDescriptorSet(VkDescriptorSet descriptorSet,
                   VkDescriptorBufferInfo const* bufferInfos,
                   std::size_t bufferInfosSize, VkDescriptorType descriptorType,
                   std::uint32_t binding);

VkCommandBufferBeginInfo commandBufferBeginInfo(
    VkCommandBufferUsageFlags flags,
    VkCommandBufferInheritanceInfo const* inheritanceInfo = nullptr);

VkSubmitInfo commandBufferSubmitInfo(VkCommandBuffer const* cmd);

} // namespace vkinit
