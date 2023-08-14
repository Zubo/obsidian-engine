#include <vk_pipeline_builder.hpp>

#include <iostream>

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass) {
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

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
  graphicsPipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCreateInfo.pNext = nullptr;

  graphicsPipelineCreateInfo.stageCount = _vkShaderStageCreateInfo.size();
  graphicsPipelineCreateInfo.pStages = _vkShaderStageCreateInfo.data();
  graphicsPipelineCreateInfo.pVertexInputState = &_vkVertexInputInfo;
  graphicsPipelineCreateInfo.pInputAssemblyState = &_vkInputAssemblyCreateInfo;
  graphicsPipelineCreateInfo.pTessellationState = nullptr;
  graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &_vkRasterizationCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &_vkMultisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pDepthStencilState =
      &_vkDepthStencilStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
  graphicsPipelineCreateInfo.pDynamicState = nullptr;
  graphicsPipelineCreateInfo.layout = _vkPipelineLayout;
  graphicsPipelineCreateInfo.renderPass = pass;
  graphicsPipelineCreateInfo.subpass = 0;
  graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline newPipeline;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                &graphicsPipelineCreateInfo, nullptr,
                                &newPipeline) != VK_SUCCESS) {
    std::cout << "Failed to create pipeline" << std::endl;
    return VK_NULL_HANDLE;
  }

  return newPipeline;
}