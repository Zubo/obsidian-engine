#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace obsidian::vk_rhi {

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _vkShaderStageCreateInfo;
  VkPipelineVertexInputStateCreateInfo _vkVertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo _vkInputAssemblyCreateInfo;
  VkPipelineDepthStencilStateCreateInfo _vkDepthStencilStateCreateInfo;
  VkViewport _vkViewport;
  VkRect2D _vkScissor;
  VkPipelineRasterizationStateCreateInfo _vkRasterizationCreateInfo;
  VkPipelineColorBlendAttachmentState _vkColorBlendAttachmentState;
  VkPipelineMultisampleStateCreateInfo _vkMultisampleStateCreateInfo;
  VkPipelineLayout _vkPipelineLayout;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};

} /*namespace obsidian::vk_rhi*/
