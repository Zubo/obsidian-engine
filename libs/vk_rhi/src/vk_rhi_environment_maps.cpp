#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

using namespace obsidian;
using namespace obsidian::vk_rhi;

rhi::ResourceIdRHI VulkanRHI::createEnvironmentMap(glm::vec3 envMapPos,
                                                   float radius) {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();

  EnvironmentMap& envMap = _environmentMaps[newResourceId];

  envMap.pos = envMapPos;
  envMap.radius = radius;

  VkExtent3D const extent{environmentMapResolution, environmentMapResolution,
                          1};

  VkImageCreateInfo colorImageCreateInfo = vkinit::imageCreateInfo(
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, extent,
      _envMapFormat, 1, 6);

  colorImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  VmaAllocationCreateInfo colorImgAllocCreateInfo = {};
  colorImgAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateImage(_vmaAllocator, &colorImageCreateInfo,
                          &colorImgAllocCreateInfo, &envMap.colorImage.vkImage,
                          &envMap.colorImage.allocation, nullptr));

  VkImageViewCreateInfo colorImageViewCreateInfo = vkinit::imageViewCreateInfo(
      envMap.colorImage.vkImage, _envMapFormat, VK_IMAGE_ASPECT_COLOR_BIT);
  colorImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  colorImageViewCreateInfo.subresourceRange.layerCount = 6;

  VK_CHECK(vkCreateImageView(_vkDevice, &colorImageViewCreateInfo, nullptr,
                             &envMap.colorImageView));

  VkImageCreateInfo depthImageCreateInfo = vkinit::imageCreateInfo(
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, extent, _depthFormat, 1, 6);

  depthImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  VmaAllocationCreateInfo depthImgAllocCreateInfo = {};
  depthImgAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateImage(_vmaAllocator, &depthImageCreateInfo,
                          &depthImgAllocCreateInfo, &envMap.depthImage.vkImage,
                          &envMap.depthImage.allocation, nullptr));

  VkImageViewCreateInfo depthImageViewCreateInfo = vkinit::imageViewCreateInfo(
      envMap.depthImage.vkImage, _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
  depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  depthImageViewCreateInfo.subresourceRange.layerCount = 6;

  VK_CHECK(vkCreateImageView(_vkDevice, &depthImageViewCreateInfo, nullptr,
                             &envMap.depthImageView));

  envMap.cameraBuffer = createBuffer(
      frameOverlap * 6 * getPaddedBufferSize(sizeof(GPUCameraData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  for (int i = 0; i < frameOverlap; ++i) {
    FrameData& frameData = _frameDataArray[i];

    std::vector<VkDescriptorImageInfo> vkShadowMapDescriptorImageInfos{
        rhi::maxLightsPerDrawPass};

    for (std::size_t j = 0; j < vkShadowMapDescriptorImageInfos.size(); ++j) {
      vkShadowMapDescriptorImageInfos[j].imageLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      vkShadowMapDescriptorImageInfos[j].imageView =
          frameData.shadowFrameBuffers[j].depthBufferImageView;
      vkShadowMapDescriptorImageInfos[j].sampler = _vkDepthSampler;
    }

    VkDescriptorBufferInfo renderPassDataDescriptorBufferInfo = {};
    renderPassDataDescriptorBufferInfo.buffer =
        _envMapRenderPassDataBuffer.buffer;
    renderPassDataDescriptorBufferInfo.offset = 0;
    renderPassDataDescriptorBufferInfo.range = sizeof(GpuRenderPassData);

    VkDescriptorBufferInfo cameraBufferInfo;
    cameraBufferInfo.buffer = envMap.cameraBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo cameraDescriptorBufferInfo = {};
    cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
    cameraDescriptorBufferInfo.offset = 0;
    cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo vkLightDataBufferInfo = {};
    vkLightDataBufferInfo.buffer = _lightDataBuffer.buffer;
    vkLightDataBufferInfo.offset = 0;
    vkLightDataBufferInfo.range = sizeof(GPULightData);

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .bindBuffer(0, renderPassDataDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(1, cameraBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImages(2, vkShadowMapDescriptorImageInfos,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(3, vkLightDataBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .declareUnusedImage(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(envMap.renderPassDescriptorSets[i]);
  }

  for (std::size_t i = 0; i < envMap.framebuffers.size(); ++i) {
    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = nullptr;
    frameBufferCreateInfo.renderPass = _envMapRenderPass.vkRenderPass;

    colorImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageViewCreateInfo.subresourceRange.baseArrayLayer = i;
    colorImageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(_vkDevice, &colorImageViewCreateInfo, nullptr,
                               &envMap.colorAttachmentImageViews[i]));

    depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.subresourceRange.baseArrayLayer = i;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(_vkDevice, &depthImageViewCreateInfo, nullptr,
                               &envMap.depthAttachmentImageViews[i]));

    std::array<VkImageView, 2> attachmentViews = {
        envMap.colorAttachmentImageViews[i],
        envMap.depthAttachmentImageViews[i]};

    frameBufferCreateInfo.attachmentCount = attachmentViews.size();
    frameBufferCreateInfo.pAttachments = attachmentViews.data();

    frameBufferCreateInfo.width = extent.width;
    frameBufferCreateInfo.height = extent.height;
    frameBufferCreateInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(_vkDevice, &frameBufferCreateInfo, nullptr,
                                 &envMap.framebuffers[i]));
  }

  envMap.pendingUpdate = true;

  return newResourceId;
}

void VulkanRHI::releaseEnvironmentMap(rhi::ResourceIdRHI envMapId) {
  if (!_environmentMaps.contains(envMapId)) {
    OBS_LOG_ERR("Trying to release environment map that doesn't exist");
    return;
  }

  _environmentMaps.at(envMapId).released = true;

  FrameData& prevFrameData = getPreviousFrameData();
  prevFrameData.pendingResourcesToDestroy.environmentMapsToDestroy.push_back(
      envMapId);
  _envMapDescriptorSetPendingUpdate = true;
}

void VulkanRHI::updateEnvironmentMap(rhi::ResourceIdRHI envMapId, glm::vec3 pos,
                                     float radius) {
  if (!_environmentMaps.contains(envMapId)) {
    OBS_LOG_ERR("Trying to update environment map that doesn't exist");
    return;
  }

  EnvironmentMap& envMap = _environmentMaps.at(envMapId);
  envMap.pos = pos;
  envMap.radius = radius;
  envMap.pendingUpdate = true;

  _envMapDescriptorSetPendingUpdate = true;
}

void VulkanRHI::applyPendingEnvironmentMapUpdates() {
  if (_envMapDescriptorSetPendingUpdate) {
    GpuEnvironmentMapDataCollection envMapData;
    envMapData.count = 0;
    auto mapIter = _environmentMaps.begin();
    std::vector<VkDescriptorImageInfo> writeImageInfos;
    writeImageInfos.reserve(maxEnvironmentMaps);

    for (std::size_t i = 0; i < _environmentMaps.size(); ++i) {
      if (!mapIter->second.released) {
        ++envMapData.count;
        envMapData.envMaps[i] = {mapIter->second.pos, mapIter->second.radius};
        VkDescriptorImageInfo& imgInfo = writeImageInfos.emplace_back();
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = mapIter->second.colorImageView;
        imgInfo.sampler = _vkLinearRepeatSampler;
      }

      ++mapIter;
    }

    uploadBufferData(0, envMapData, _envMapDataBuffer);

    if (writeImageInfos.size()) {
      VkWriteDescriptorSet writeEnvGlobalDescriptorSet = {};
      writeEnvGlobalDescriptorSet.sType =
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeEnvGlobalDescriptorSet.pNext = nullptr;

      writeEnvGlobalDescriptorSet.dstSet = _vkGlobalDescriptorSet;
      writeEnvGlobalDescriptorSet.dstBinding = 2;
      writeEnvGlobalDescriptorSet.dstArrayElement = 0;
      writeEnvGlobalDescriptorSet.descriptorType =
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeEnvGlobalDescriptorSet.descriptorCount = writeImageInfos.size();
      writeEnvGlobalDescriptorSet.pImageInfo = writeImageInfos.data();

      vkUpdateDescriptorSets(_vkDevice, 1, &writeEnvGlobalDescriptorSet, 0,
                             nullptr);
    }

    _envMapDescriptorSetPendingUpdate = false;
  }
}
