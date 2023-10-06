#include <obsidian/vk_rhi/vk_initializers.hpp>

namespace obsidian::vk_rhi::vkinit {

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
rasterizationCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode) {
  VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
  rasterizationCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationCreateInfo.pNext = nullptr;
  rasterizationCreateInfo.depthClampEnable = VK_FALSE;
  rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationCreateInfo.polygonMode = polygonMode;
  rasterizationCreateInfo.cullMode = cullMode;
  rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
  rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
  rasterizationCreateInfo.depthBiasClamp = 0.0f;
  rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
  rasterizationCreateInfo.lineWidth = 1.0f;

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
  multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
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

VkSamplerCreateInfo samplerCreateInfo(VkFilter filter,
                                      VkSamplerMipmapMode mipmapMode,
                                      VkSamplerAddressMode addressMode,
                                      std::optional<float> maxAnisotropy) {
  VkSamplerCreateInfo vkSamplerCreateInfo = {};
  vkSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  vkSamplerCreateInfo.pNext = nullptr;

  vkSamplerCreateInfo.magFilter = filter;
  vkSamplerCreateInfo.minFilter = filter;
  vkSamplerCreateInfo.mipmapMode = mipmapMode;
  vkSamplerCreateInfo.addressModeU = addressMode;
  vkSamplerCreateInfo.addressModeV = addressMode;
  vkSamplerCreateInfo.addressModeW = addressMode;
  vkSamplerCreateInfo.anisotropyEnable = maxAnisotropy.has_value();

  if (maxAnisotropy) {
    vkSamplerCreateInfo.maxAnisotropy = *maxAnisotropy;
  }

  return vkSamplerCreateInfo;
}

VkImageMemoryBarrier layoutImageBarrier(VkImage image, VkImageLayout oldLayout,
                                        VkImageLayout newLayout,
                                        VkImageAspectFlagBits aspectMask) {
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = nullptr;

  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = aspectMask;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  return barrier;
}

VkFramebufferCreateInfo framebufferCreateInfo(VkRenderPass renderPass,
                                              uint32_t attachmentCount,
                                              VkImageView const* attachments,
                                              uint32_t width, uint32_t height,
                                              uint32_t layers) {
  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.pNext = nullptr;

  framebufferCreateInfo.renderPass = renderPass;
  framebufferCreateInfo.attachmentCount = attachmentCount;
  framebufferCreateInfo.pAttachments = attachments;
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = layers;

  return framebufferCreateInfo;
}

VkAttachmentDescription colorAttachmentDescription(VkFormat format,
                                                   VkImageLayout finalLayout) {
  VkAttachmentDescription attachmentDescription = {};

  attachmentDescription.format = format;
  attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescription.finalLayout = finalLayout;

  return attachmentDescription;
}

VkAttachmentDescription depthAttachmentDescription(VkFormat format) {
  VkAttachmentDescription attachmentDescription = {};

  attachmentDescription.format = format;
  attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescription.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  return attachmentDescription;
}

} /*namespace obsidian::vk_rhi::vkinit*/
