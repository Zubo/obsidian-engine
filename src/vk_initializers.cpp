#include <vk_initializers.hpp>
#include <vulkan/vulkan_core.h>

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
  vkCommandBufferAllocateInfo.level = level;
  vkCommandBufferAllocateInfo.commandBufferCount = count;

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
  rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags const flags) {
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;

  return info;
}

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags const flags) {
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;

  return info;
}

VkImageCreateInfo imageCreateInfo(VkImageUsageFlags const usageFlags,
                                  VkExtent3D const extent,
                                  VkFormat const format) {
  VkImageCreateInfo vkImageCreateInfo = {};
  vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  vkImageCreateInfo.pNext = nullptr;
  vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  vkImageCreateInfo.format = format;
  vkImageCreateInfo.extent = extent;
  vkImageCreateInfo.mipLevels = 1;
  vkImageCreateInfo.arrayLayers = 1;
  vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  vkImageCreateInfo.usage = usageFlags;

  return vkImageCreateInfo;
}

VkImageViewCreateInfo
imageViewCreateInfo(VkImage const image, VkFormat const format,
                    VkImageAspectFlags const imageAspectFlags) {

  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.pNext = nullptr;

  imageViewCreateInfo.image = image;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCreateInfo.format = format;
  imageViewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;
  imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
  imageViewCreateInfo.subresourceRange.layerCount = 1;

  return imageViewCreateInfo;
}

VkPipelineDepthStencilStateCreateInfo
depthStencilStateCreateInfo(bool const depthTestEnable) {
  VkPipelineDepthStencilStateCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  createInfo.pNext = nullptr;

  createInfo.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
  createInfo.depthWriteEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
  createInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  createInfo.depthBoundsTestEnable = VK_FALSE;
  createInfo.stencilTestEnable = VK_FALSE;

  return createInfo;
}

VkDescriptorSetLayoutBinding
descriptorSetLayoutBinding(std::uint32_t binding,
                           VkDescriptorType descriptorType,
                           VkShaderStageFlags stageFlags) {
  VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
  descriptorSetLayoutBinding.binding = binding;
  descriptorSetLayoutBinding.descriptorType = descriptorType;
  descriptorSetLayoutBinding.descriptorCount = 1;
  descriptorSetLayoutBinding.stageFlags = stageFlags;
  descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

  return descriptorSetLayoutBinding;
}

VkDescriptorSetLayoutCreateInfo
descriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding const* pBindings,
                              std::uint32_t bindingCount) {

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
  descriptorSetLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCreateInfo.pNext = nullptr;

  descriptorSetLayoutCreateInfo.flags = 0;
  descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
  descriptorSetLayoutCreateInfo.pBindings = pBindings;

  return descriptorSetLayoutCreateInfo;
}

VkDescriptorSetAllocateInfo
descriptorSetAllocateInfo(VkDescriptorPool descriptorPool,
                          VkDescriptorSetLayout const* descriptorSetLayouts,
                          std::uint32_t descriptorSetLayoutCount) {
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.pNext = nullptr;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = descriptorSetLayoutCount;
  descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts;

  return descriptorSetAllocateInfo;
}

VkWriteDescriptorSet
writeDescriptorSet(VkDescriptorSet descriptorSet,
                   VkDescriptorBufferInfo const* bufferInfos,
                   std::size_t bufferInfosSize, VkDescriptorType descriptorType,
                   std::uint32_t binding) {
  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.pNext = nullptr;
  writeDescriptorSet.dstSet = descriptorSet;
  writeDescriptorSet.dstBinding = binding;
  writeDescriptorSet.dstArrayElement = 0;
  writeDescriptorSet.descriptorCount = bufferInfosSize;
  writeDescriptorSet.descriptorType = descriptorType;
  writeDescriptorSet.pBufferInfo = bufferInfos;

  return writeDescriptorSet;
}

VkCommandBufferBeginInfo
commandBufferBeginInfo(VkCommandBufferUsageFlags flags,
                       VkCommandBufferInheritanceInfo const* inheritanceInfo) {
  VkCommandBufferBeginInfo commandBufferBeginInfo = {};

  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.pNext = nullptr;

  commandBufferBeginInfo.flags = flags;
  commandBufferBeginInfo.pInheritanceInfo = inheritanceInfo;

  return commandBufferBeginInfo;
}

VkSubmitInfo commandBufferSubmitInfo(VkCommandBuffer const* cmd) {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  submitInfo.pNext = nullptr;

  submitInfo.waitSemaphoreCount = 0;
  submitInfo.pWaitSemaphores = nullptr;

  submitInfo.pWaitDstStageMask = nullptr;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = cmd;

  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores = nullptr;

  return submitInfo;
}

} // namespace vkinit
