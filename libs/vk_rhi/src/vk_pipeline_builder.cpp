#include <obsidian/core/logging.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>

using namespace obsidian::vk_rhi;

VkPipeline PipelineBuilder::buildPipeline(VkDevice device,
                                          RenderPass const& pass) {
  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
  viewportStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateCreateInfo.pNext = nullptr;
  viewportStateCreateInfo.viewportCount = 1;
  viewportStateCreateInfo.pViewports = &_vkViewport;
  viewportStateCreateInfo.scissorCount = 1;
  viewportStateCreateInfo.pScissors = &_vkScissor;

  VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
  colorBlendingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendingCreateInfo.pNext = nullptr;
  colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
  colorBlendingCreateInfo.attachmentCount = 1;
  colorBlendingCreateInfo.pAttachments = &_vkColorBlendAttachmentState;

  VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
  dynamicStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicStateCreateInfo.pNext = nullptr;
  dynamicStateCreateInfo.dynamicStateCount = _vkDynamicStates.size();
  dynamicStateCreateInfo.pDynamicStates = _vkDynamicStates.data();

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
  graphicsPipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCreateInfo.pNext = nullptr;

  _vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo(pass.sampleCount);

  graphicsPipelineCreateInfo.stageCount = _vkShaderStageCreateInfos.size();
  graphicsPipelineCreateInfo.pStages = _vkShaderStageCreateInfos.data();
  graphicsPipelineCreateInfo.pInputAssemblyState = &_vkInputAssemblyCreateInfo;
  graphicsPipelineCreateInfo.pTessellationState = nullptr;
  graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &_vkRasterizationCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &_vkMultisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pDepthStencilState =
      &_vkDepthStencilStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
  graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
  graphicsPipelineCreateInfo.layout = _vkPipelineLayout;
  graphicsPipelineCreateInfo.renderPass = pass.vkRenderPass;
  graphicsPipelineCreateInfo.subpass = 0;
  graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline newPipeline;

  VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                     &graphicsPipelineCreateInfo, nullptr,
                                     &newPipeline));

  return newPipeline;
}
