#pragma once

#include <obsidian/vk_rhi/vk_types.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace obsidian::vk_rhi {

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _vkShaderStageCreateInfo;
  VkPipelineInputAssemblyStateCreateInfo _vkInputAssemblyCreateInfo;
  VkPipelineDepthStencilStateCreateInfo _vkDepthStencilStateCreateInfo;
  VkViewport _vkViewport;
  VkRect2D _vkScissor;
  VkPipelineRasterizationStateCreateInfo _vkRasterizationCreateInfo;
  VkPipelineColorBlendAttachmentState _vkColorBlendAttachmentState;
  VkPipelineMultisampleStateCreateInfo _vkMultisampleStateCreateInfo;
  VkPipelineLayout _vkPipelineLayout;
  VertexInputDescription _vertexInputAttributeDescription;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};

} /*namespace obsidian::vk_rhi*/
