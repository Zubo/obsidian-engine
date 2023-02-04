#include <vk_initializers.hpp>

namespace vkinit {

VkCommandPoolCreateInfo commandPoolCreateInfo(std::uint32_t queueFamilyIndex,
                                              VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
  vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  vkCommandPoolCreateInfo.pNext = nullptr;
  vkCommandPoolCreateInfo.flags = flags;
  vkCommandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

  return vkCommandPoolCreateInfo;
}

VkCommandBufferAllocateInfo
commandBufferAllocateInfo(VkCommandPool const pool, std::uint32_t const count,
                          VkCommandBufferLevel const level) {
  VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo;
  vkCommandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  vkCommandBufferAllocateInfo.pNext = nullptr;
  vkCommandBufferAllocateInfo.commandPool = pool;
  vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkCommandBufferAllocateInfo.commandBufferCount = 1;

  return vkCommandBufferAllocateInfo;
}

VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits shaderStageFlags,
                              VkShaderModule shaderModule) {
  VkPipelineShaderStageCreateInfo vkShaderStageCreateInfo = {};
  vkShaderStageCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vkShaderStageCreateInfo.pNext = nullptr;

  vkShaderStageCreateInfo.stage = shaderStageFlags;
  vkShaderStageCreateInfo.module = shaderModule;
  vkShaderStageCreateInfo.pName = "main";
  vkShaderStageCreateInfo.pSpecializationInfo = nullptr;

  return vkShaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo() {
  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
  vertexInputStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputStateCreateInfo.pNext = nullptr;
  vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
  vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;

  return vertexInputStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo
inputAssemblyCreateInfo(VkPrimitiveTopology primitiveTopology) {
  VkPipelineInputAssemblyStateCreateInfo inputAssebmlyStateCreateInfo = {};
  inputAssebmlyStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssebmlyStateCreateInfo.pNext = nullptr;

  inputAssebmlyStateCreateInfo.topology = primitiveTopology;
  inputAssebmlyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

  return inputAssebmlyStateCreateInfo;
}

VkPipelineRasterizationStateCreateInfo
rasterizationCreateInfo(VkPolygonMode polygonMode) {
  VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
  rasterizationCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationCreateInfo.pNext = nullptr;
  rasterizationCreateInfo.depthClampEnable = VK_FALSE;
  rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationCreateInfo.polygonMode = polygonMode;
  rasterizationCreateInfo.cullMode = VK_CULL_MODE_NONE;
  rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
  rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
  rasterizationCreateInfo.depthBiasClamp = 0.0f;
  rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;

  return rasterizationCreateInfo;
}

VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo() {
  VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
  multisampleStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleStateCreateInfo.pNext = nullptr;

  multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
  multisampleStateCreateInfo.minSampleShading = 1.0f;
  multisampleStateCreateInfo.pSampleMask = nullptr;
  multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
  multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

  return multisampleStateCreateInfo;
}

VkPipelineColorBlendAttachmentState colorBlendAttachmentState() {
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
  colorBlendAttachmentState.blendEnable = VK_FALSE;
  colorBlendAttachmentState.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  return colorBlendAttachmentState;
}

VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo() {
  VkPipelineLayoutCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;

  return info;
}

} // namespace vkinit
