#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/utils/visitor.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/task/task_type.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_deletion_queue.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <numeric>
#include <optional>
#include <variant>
#include <vulkan/vulkan_core.h>

using namespace obsidian;
using namespace obsidian::vk_rhi;

void VulkanRHI::waitDeviceIdle() const { vkDeviceWaitIdle(_vkDevice); }

rhi::ResourceRHI&
VulkanRHI::uploadTexture(rhi::UploadTextureRHI uploadTextureInfoRHI) {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Texture& newTexture = _textures[newResourceId];

  newTexture.resource.id = newResourceId;
  newTexture.resource.state = rhi::ResourceState::uploading;
  newTexture.resource.refCount = 1;

  VkImageUsageFlags const imageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkExtent3D extent = {};
  extent.width = uploadTextureInfoRHI.width;
  extent.height = uploadTextureInfoRHI.height;
  extent.depth = 1;
  VkFormat const format = getVkTextureFormat(uploadTextureInfoRHI.format);

  VkImageCreateInfo vkImgCreateInfo =
      vkinit::imageCreateInfo(imageUsageFlags, extent, format);

  VmaAllocationCreateInfo imgAllocationCreateInfo = {};
  imgAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateImage(_vmaAllocator, &vkImgCreateInfo,
                          &imgAllocationCreateInfo, &newTexture.image.vkImage,
                          &newTexture.image.allocation, nullptr));

  VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageViewCreateInfo(
      newTexture.image.vkImage, getVkTextureFormat(uploadTextureInfoRHI.format),
      VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr,
                             &newTexture.imageView));

  assert(_taskExecutor);

  _taskExecutor->enqueue(
      task::TaskType::rhiUpload,
      [this, &newTexture, extent, info = std::move(uploadTextureInfoRHI)]() {
        std::size_t const size =
            info.width * info.height * core::getFormatPixelSize(info.format);

        AllocatedBuffer stagingBuffer = createBuffer(
            size, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_HOST,

            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        void* mappedMemory;
        vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

        {
          ZoneScopedN("Upload Texture");
          info.unpackFunc(reinterpret_cast<char*>(mappedMemory));
        }

        vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

        immediateSubmit(_transferQueueFamilyIndex, [&extent, &newTexture,
                                                    &stagingBuffer](
                                                       VkCommandBuffer cmd) {
          VkImageMemoryBarrier vkImgBarrierToTransfer =
              vkinit::layoutImageBarrier(newTexture.image.vkImage,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
          vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
          vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

          vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                               nullptr, 1, &vkImgBarrierToTransfer);

          VkBufferImageCopy vkBufferImgCopy = {};
          vkBufferImgCopy.imageExtent = extent;
          vkBufferImgCopy.imageSubresource.aspectMask =
              VK_IMAGE_ASPECT_COLOR_BIT;
          vkBufferImgCopy.imageSubresource.layerCount = 1;

          vkCmdCopyBufferToImage(
              cmd, stagingBuffer.buffer, newTexture.image.vkImage,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBufferImgCopy);
        });

        vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                         stagingBuffer.allocation);

        immediateSubmit(_graphicsQueueFamilyIndex, [&newTexture](
                                                       VkCommandBuffer cmd) {
          VkImageMemoryBarrier vkImageBarrierToRead =
              vkinit::layoutImageBarrier(
                  newTexture.image.vkImage,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_ASPECT_COLOR_BIT);

          vkImageBarrierToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          vkImageBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

          vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &vkImageBarrierToRead);
        });

        rhi::ResourceState expected = rhi::ResourceState::uploading;

        if (!newTexture.resource.state.compare_exchange_strong(
                expected, rhi::ResourceState::uploaded)) {
          OBS_LOG_ERR("Texture resource state expected to be uploading.");
        }
      });

  return newTexture.resource;
}

void VulkanRHI::releaseTexture(rhi::ResourceIdRHI resourceIdRHI) {
  Texture& tex = _textures[resourceIdRHI];
  --tex.resource.refCount;
}

rhi::ResourceRHI& VulkanRHI::uploadMesh(rhi::UploadMeshRHI meshInfo) {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Mesh& mesh = _meshes[newResourceId];
  mesh.resource.id = newResourceId;
  mesh.resource.state = rhi::ResourceState::uploading;
  mesh.resource.refCount = 1;

  assert(_taskExecutor);

  mesh.vertexBuffer = createBuffer(meshInfo.vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  mesh.vertexCount = meshInfo.vertexCount;

  std::size_t const totalIndexBufferSize = std::accumulate(
      meshInfo.indexBufferSizes.cbegin(), meshInfo.indexBufferSizes.cend(), 0);

  mesh.indexBuffer = createBuffer(totalIndexBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  mesh.indexBufferSizes = meshInfo.indexBufferSizes;
  mesh.indexCount = meshInfo.indexCount;

  _taskExecutor->enqueue(
      task::TaskType::rhiUpload,
      [this, totalIndexBufferSize, &mesh, info = std::move(meshInfo)]() {
        AllocatedBuffer stagingBuffer = createBuffer(
            info.vertexBufferSize + totalIndexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        void* mappedMemory;
        vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

        {
          ZoneScopedN("Unpack Mesh");
          info.unpackFunc(reinterpret_cast<char*>(mappedMemory));
        }

        vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

        immediateSubmit(_transferQueueFamilyIndex, [this, &stagingBuffer, &mesh,
                                                    info, totalIndexBufferSize](
                                                       VkCommandBuffer cmd) {
          VkBufferCopy vkVertexBufferCopy = {};
          vkVertexBufferCopy.srcOffset = 0;
          vkVertexBufferCopy.dstOffset = 0;
          vkVertexBufferCopy.size = info.vertexBufferSize;
          vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer,
                          1, &vkVertexBufferCopy);

          VkBufferCopy vkIndexBufferCopy = {};
          vkIndexBufferCopy.srcOffset = info.vertexBufferSize;
          vkIndexBufferCopy.dstOffset = 0;
          vkIndexBufferCopy.size = totalIndexBufferSize;

          vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.indexBuffer.buffer, 1,
                          &vkIndexBufferCopy);
        });

        vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                         stagingBuffer.allocation);

        rhi::ResourceState expected = rhi::ResourceState::uploading;

        if (!mesh.resource.state.compare_exchange_strong(
                expected, rhi::ResourceState::uploaded)) {
          OBS_LOG_ERR("Mesh resource state expected to be uploading.");
        }
      });

  return mesh.resource;
}

void VulkanRHI::releaseMesh(rhi::ResourceIdRHI resourceIdRHI) {
  Mesh& mesh = _meshes[resourceIdRHI];
  --mesh.resource.refCount;
}

rhi::ResourceRHI& VulkanRHI::uploadShader(rhi::UploadShaderRHI uploadShader) {
  rhi::ResourceIdRHI newResourceId = consumeNewResourceId();
  Shader& shader = _shaderModules[newResourceId];
  shader.resource.id = newResourceId;
  shader.resource.state = rhi::ResourceState::uploading;
  shader.resource.refCount = 1;

  std::vector<std::uint32_t> buffer(
      (uploadShader.shaderDataSize + sizeof(std::uint32_t) - 1) /
      sizeof(std::uint32_t));

  {
    ZoneScopedN("Unpack Shader");
    uploadShader.unpackFunc(reinterpret_cast<char*>(buffer.data()));
  }

  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  shaderModuleCreateInfo.codeSize = uploadShader.shaderDataSize;
  shaderModuleCreateInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_vkDevice, &shaderModuleCreateInfo, nullptr,
                           &shaderModule)) {
    OBS_LOG_ERR("Failed to load shader.");
    shader.resource.state = rhi::ResourceState::invalid;
    return shader.resource;
  }

  shader.vkShaderModule = shaderModule;
  shader.resource.state = rhi::ResourceState::uploaded;

  return shader.resource;
}

void VulkanRHI::releaseShader(rhi::ResourceIdRHI resourceIdRHI) {
  Shader& shader = _shaderModules[resourceIdRHI];
  --shader.resource.refCount;
}

template <typename MaterialDataT>
void VulkanRHI::createAndBindMaterialDataBuffer(
    MaterialDataT const& materialData, DescriptorBuilder& builder,
    VkDescriptorBufferInfo& bufferInfo) {
  AllocatedBuffer materialDataBuffer = createBuffer(
      getPaddedBufferSize(sizeof(MaterialDataT)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  uploadBufferData(0, materialData, materialDataBuffer);

  _deletionQueue.pushFunction([this, materialDataBuffer]() {
    vmaDestroyBuffer(_vmaAllocator, materialDataBuffer.buffer,
                     materialDataBuffer.allocation);
  });

  bufferInfo.buffer = materialDataBuffer.buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(MaterialDataT);

  builder.bindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_FRAGMENT_BIT);
}

rhi::ResourceRHI&
VulkanRHI::uploadMaterial(rhi::UploadMaterialRHI uploadMaterial) {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Material& newMaterial = _materials[newResourceId];
  newMaterial.resource.id = newResourceId;
  newMaterial.resource.state = rhi::ResourceState::uploading;
  newMaterial.resource.refCount = 1;

  PipelineBuilder& pipelineBuilder =
      _pipelineBuilders[uploadMaterial.materialType];

  Shader& shaderModule = _shaderModules[uploadMaterial.shaderId];
  ++shaderModule.resource.refCount;
  newMaterial.resourceDependencies.push_back(&shaderModule.resource);

  pipelineBuilder._vkShaderStageCreateInfos.clear();
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule.vkShaderModule));

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule.vkShaderModule));

  newMaterial.vkPipelineLayout = pipelineBuilder._vkPipelineLayout;
  newMaterial.vkPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _mainRenderPass.vkRenderPass);
  newMaterial.transparent = uploadMaterial.transparent;

  DescriptorBuilder builder = DescriptorBuilder::begin(
      _vkDevice, _descriptorAllocator, _descriptorLayoutCache);

  VkDescriptorBufferInfo materialDataBufferInfo;
  VkDescriptorImageInfo diffuseTexImageInfo;
  VkDescriptorImageInfo normalMapTexImageInfo;

  if (uploadMaterial.materialType == core::MaterialType::lit) {
    GPULitMaterialData materialData;
    materialData.hasDiffuseTex.value =
        uploadMaterial.diffuseTextureId != rhi::rhiIdUninitialized;
    materialData.hasNormalMap.value =
        uploadMaterial.normalTextureId != rhi::rhiIdUninitialized;
    materialData.ambientColor = uploadMaterial.ambientColor;
    materialData.diffuseColor = uploadMaterial.diffuseColor;
    materialData.specularColor = uploadMaterial.specularColor;
    materialData.shininess = uploadMaterial.shininess;

    createAndBindMaterialDataBuffer(materialData, builder,
                                    materialDataBufferInfo);

  } else {
    GPUUnlitMaterialData materialData;
    materialData.hasDiffuseTex.value =
        uploadMaterial.diffuseTextureId != rhi::rhiIdUninitialized;
    materialData.diffuseColor = uploadMaterial.diffuseColor;

    VkDescriptorBufferInfo materialDataBufferInfo;

    createAndBindMaterialDataBuffer(materialData, builder,
                                    materialDataBufferInfo);
  }

  bool const hasDiffuseTex =
      uploadMaterial.diffuseTextureId != rhi::rhiIdUninitialized;

  if (hasDiffuseTex) {
    Texture& diffuseTexture = _textures[uploadMaterial.diffuseTextureId];

    ++diffuseTexture.resource.refCount;
    newMaterial.resourceDependencies.push_back(&diffuseTexture.resource);

    diffuseTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    diffuseTexImageInfo.imageView = diffuseTexture.imageView;
    diffuseTexImageInfo.sampler = _vkLinearRepeatSampler;

    builder.bindImage(1, diffuseTexImageInfo,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
  } else {
    builder.declareUnusedImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  bool const hasNormalMap =
      uploadMaterial.normalTextureId != rhi::rhiIdUninitialized;

  if (uploadMaterial.materialType == core::MaterialType::lit) {
    if (hasNormalMap) {
      normalMapTexImageInfo.imageLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      normalMapTexImageInfo.sampler = _vkLinearRepeatSampler;
      Texture& normalMapTexture = _textures[uploadMaterial.normalTextureId];

      ++normalMapTexture.resource.refCount;
      newMaterial.resourceDependencies.push_back(&normalMapTexture.resource);

      normalMapTexImageInfo.imageView = normalMapTexture.imageView;

      builder.bindImage(2, normalMapTexImageInfo,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
    } else {
      builder.declareUnusedImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
    }
  }

  builder.build(newMaterial.vkDescriptorSet);

  rhi::ResourceState expected = rhi::ResourceState::uploading;

  if (!newMaterial.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploaded)) {
    OBS_LOG_ERR("Material resource state expected to be uploading.");
  }

  return newMaterial.resource;
}

void VulkanRHI::releaseMaterial(rhi::ResourceIdRHI resourceIdRHI) {
  Material& material = _materials[resourceIdRHI];
  --material.resource.refCount;

  if (material.resource.refCount == 0) {
    for (rhi::ResourceRHI* dep : material.resourceDependencies) {
      --(dep->refCount);
    }
  }
}

void VulkanRHI::destroyUnreferencedResources() {
  std::vector<rhi::ResourceIdRHI> eraseIds;

  // clear materials
  for (auto& mat : _materials) {
    if (!mat.second.resource.refCount) {
      vkDestroyPipeline(_vkDevice, mat.second.vkPipeline, nullptr);
      eraseIds.push_back(mat.second.resource.id);
    }
  }

  for (rhi::ResourceIdRHI id : eraseIds) {
    _materials.erase(id);
  }

  eraseIds.clear();

  // clear textures
  for (auto& tex : _textures) {
    if (!tex.second.resource.refCount) {
      vkDestroyImageView(_vkDevice, tex.second.imageView, nullptr);
      vmaDestroyImage(_vmaAllocator, tex.second.image.vkImage,
                      tex.second.image.allocation);
      eraseIds.push_back(tex.second.resource.id);
    }
  }

  for (rhi::ResourceIdRHI id : eraseIds) {
    _textures.erase(id);
  }

  eraseIds.clear();

  // clear shaders
  for (auto& shader : _shaderModules) {
    if (!shader.second.resource.refCount) {
      vkDestroyShaderModule(_vkDevice, shader.second.vkShaderModule, nullptr);
      eraseIds.push_back(shader.second.resource.id);
    }
  }

  for (rhi::ResourceIdRHI id : eraseIds) {
    _shaderModules.erase(id);
  }

  eraseIds.clear();

  // clear meshes
  for (auto& mesh : _meshes) {
    if (!mesh.second.resource.refCount) {
      vmaDestroyBuffer(_vmaAllocator, mesh.second.vertexBuffer.buffer,
                       mesh.second.vertexBuffer.allocation);

      vmaDestroyBuffer(_vmaAllocator, mesh.second.indexBuffer.buffer,
                       mesh.second.indexBuffer.allocation);

      eraseIds.push_back(mesh.second.resource.id);
    }
  }

  for (rhi::ResourceIdRHI id : eraseIds) {
    _meshes.erase(id);
  }

  eraseIds.clear();
}

void VulkanRHI::submitDrawCall(rhi::DrawCall const& drawCall) {
  VKDrawCall vkDrawCall;
  vkDrawCall.model = drawCall.transform;
  vkDrawCall.mesh = &_meshes[drawCall.meshId];
  for (std::size_t i = 0; i < drawCall.materialIds.size(); ++i) {
    vkDrawCall.material = &_materials[drawCall.materialIds[i]];
    vkDrawCall.indexBufferInd = i;
    if (vkDrawCall.material->transparent) {
      _transparentDrawCallQueue.push_back(vkDrawCall);
    } else {
      _drawCallQueue.push_back(vkDrawCall);
    }
  }
}

void VulkanRHI::submitLight(rhi::LightSubmitParams const& light) {
  std::visit(
      core::visitor(
          [this](rhi::DirectionalLightParams const& l) { submitLight(l); },
          [this](rhi::SpotlightParams const& l) { submitLight(l); }),
      light);
}

VkInstance VulkanRHI::getInstance() const { return _vkInstance; }

void VulkanRHI::setSurface(VkSurfaceKHR surface) { _vkSurface = surface; }

void VulkanRHI::updateExtent(rhi::WindowExtentRHI newExtent) {
  _pendingExtentUpdate = newExtent;
}

void VulkanRHI::immediateSubmit(
    std::uint32_t queueInd,
    std::function<void(VkCommandBuffer cmd)>&& function) {
  thread_local std::unordered_map<std::uint32_t, ImmediateSubmitContext>
      immediateSubmitContext;
  thread_local bool contextsInitialized = false;

  if (!contextsInitialized) {
    for (auto const& q : _gpuQueues) {
      initImmediateSubmitContext(immediateSubmitContext[q.first], q.first);
    }

    contextsInitialized = true;
  }

  VkCommandBufferBeginInfo commandBufferBeginInfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(
      vkBeginCommandBuffer(immediateSubmitContext[queueInd].vkCommandBuffer,
                           &commandBufferBeginInfo));

  function(immediateSubmitContext[queueInd].vkCommandBuffer);

  VK_CHECK(
      vkEndCommandBuffer(immediateSubmitContext[queueInd].vkCommandBuffer));

  VkSubmitInfo submit = vkinit::commandBufferSubmitInfo(
      &immediateSubmitContext[queueInd].vkCommandBuffer);

  {
    std::scoped_lock l{_gpuQueueMutexes[queueInd]};
    VK_CHECK(vkQueueSubmit(_gpuQueues[queueInd], 1, &submit,
                           immediateSubmitContext[queueInd].vkFence));
  }

  vkWaitForFences(_vkDevice, 1, &immediateSubmitContext[queueInd].vkFence,
                  VK_TRUE, 9999999999);
  vkResetFences(_vkDevice, 1, &immediateSubmitContext[queueInd].vkFence);

  VK_CHECK(vkResetCommandPool(
      _vkDevice, immediateSubmitContext[queueInd].vkCommandPool, 0));
}

FrameData& VulkanRHI::getCurrentFrameData() {
  std::size_t const currentFrameDataInd = _frameNumber % frameOverlap;
  return _frameDataArray[currentFrameDataInd];
}

AllocatedBuffer
VulkanRHI::createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
                        VmaMemoryUsage memoryUsage,
                        VmaAllocationCreateFlags allocationCreateFlags,
                        VmaAllocationInfo* outAllocationInfo) const {
  VkBufferCreateInfo vkBufferCreateInfo = {};
  vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vkBufferCreateInfo.pNext = nullptr;
  vkBufferCreateInfo.size = bufferSize;
  vkBufferCreateInfo.usage = usage;

  VmaAllocationCreateInfo vmaAllocationCreateInfo = {};
  vmaAllocationCreateInfo.usage = memoryUsage;
  vmaAllocationCreateInfo.flags = allocationCreateFlags;

  AllocatedBuffer allocatedBuffer;
  VK_CHECK(vmaCreateBuffer(_vmaAllocator, &vkBufferCreateInfo,
                           &vmaAllocationCreateInfo, &allocatedBuffer.buffer,
                           &allocatedBuffer.allocation, outAllocationInfo));

  return allocatedBuffer;
}

std::size_t VulkanRHI::getPaddedBufferSize(std::size_t originalSize) const {
  std::size_t const minbufferOffset =
      _vkPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
  if (!minbufferOffset)
    return originalSize;

  return (originalSize + minbufferOffset - 1) & (~(minbufferOffset - 1));
}

rhi::ResourceIdRHI VulkanRHI::consumeNewResourceId() {
  return _nextResourceId++;
}

int VulkanRHI::getNextAvailableShadowMapIndex() {
  int next = _submittedDirectionalLights.size() + _submittedSpotlights.size();

  if (next >= rhi::maxLightsPerDrawPass) {
    return -1;
  }

  return next;
}

void VulkanRHI::submitLight(
    rhi::DirectionalLightParams const& directionalLight) {
  _submittedDirectionalLights.push_back(
      {directionalLight, getNextAvailableShadowMapIndex()});
}

void VulkanRHI::submitLight(rhi::SpotlightParams const& spotlight) {
  _submittedSpotlights.push_back({spotlight, getNextAvailableShadowMapIndex()});
}

std::vector<ShadowPassParams> VulkanRHI::getSubmittedShadowPassParams() const {
  std::vector<ShadowPassParams> result;

  for (rhi::DirectionalLight const& dirLight : _submittedDirectionalLights) {
    if (dirLight.assignedShadowMapInd < 0) {
      continue;
    }

    ShadowPassParams& param = result.emplace_back();
    param.shadowMapIndex = dirLight.assignedShadowMapInd;
    param.gpuCameraData =
        getDirectionalLightCameraData(dirLight.directionalLight.direction);
  }

  for (rhi::Spotlight const& spotlight : _submittedSpotlights) {
    if (spotlight.assignedShadowMapInd < 0) {
      continue;
    }

    ShadowPassParams& param = result.emplace_back();
    param.shadowMapIndex = spotlight.assignedShadowMapInd;
    param.gpuCameraData = getSpotlightCameraData(
        spotlight.spotlight.position, spotlight.spotlight.direction,
        spotlight.spotlight.fadeoutAngleRad);
  }

  return result;
}

GPUCameraData
VulkanRHI::getSceneCameraData(rhi::SceneGlobalParams const& sceneParams) const {
  glm::mat4 view = glm::mat4{1.f};
  view = glm::rotate(view, -sceneParams.cameraRotationRad.x, {1.f, 0.f, 0.f});
  view = glm::rotate(view, -sceneParams.cameraRotationRad.y, {0.f, 1.f, 0.f});
  view = glm::translate(view, -sceneParams.cameraPos);

  glm::mat4 proj =
      glm::perspective(glm::radians(60.f),
                       static_cast<float>(_vkbSwapchain.extent.width) /
                           _vkbSwapchain.extent.height,
                       0.1f, 400.f);
  proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  proj = glm::scale(glm::mat4{1.0f}, glm::vec3{1.f, 1.f, 0.5f}) *
         glm::translate(glm::mat4{1.0f}, glm::vec3{0.f, 0.f, 1.f}) * proj;
  glm::mat4 const viewProjection = proj * view;

  GPUCameraData gpuCameraData;
  gpuCameraData.view = view;
  gpuCameraData.proj = proj;
  gpuCameraData.viewProj = proj * view;

  return gpuCameraData;
}

GPULightData VulkanRHI::getGPULightData() const {
  GPULightData lightData;

  for (std::size_t i = 0; i < _submittedDirectionalLights.size(); ++i) {
    if (_submittedDirectionalLights[i].assignedShadowMapInd < 0) {
      continue;
    }
    lightData.directionalLights[i].direction = {
        _submittedDirectionalLights[i].directionalLight.direction, 1.0f};
    lightData.directionalLights[i].color = {
        _submittedDirectionalLights[i].directionalLight.color, 1.0f};

    GPUCameraData const cameraData = getDirectionalLightCameraData(
        _submittedDirectionalLights[i].directionalLight.direction);

    lightData.directionalLights[i].viewProjection = cameraData.viewProj;
    float const intensity =
        _submittedDirectionalLights[i].directionalLight.intensity;
    lightData.directionalLights[i].intensity = {intensity, intensity, intensity,
                                                1.0f};
    lightData.directionalLightShadowMapIndices[i].value =
        _submittedDirectionalLights[i].assignedShadowMapInd;
  }

  lightData.directionalLightCount = _submittedDirectionalLights.size();

  for (std::size_t i = 0; i < _submittedSpotlights.size(); ++i) {
    if (_submittedSpotlights[i].assignedShadowMapInd < 0) {
      continue;
    }

    lightData.spotlights[i].direction = {
        _submittedSpotlights[i].spotlight.direction, 1.0f};
    lightData.spotlights[i].position = {
        _submittedSpotlights[i].spotlight.position, 1.0f};
    lightData.spotlights[i].color = {_submittedSpotlights[i].spotlight.color,
                                     1.0f};

    GPUCameraData const cameraData = getSpotlightCameraData(
        _submittedSpotlights[i].spotlight.position,
        _submittedSpotlights[i].spotlight.direction,
        _submittedSpotlights[i].spotlight.fadeoutAngleRad);

    lightData.spotlights[i].viewProjection = cameraData.viewProj;

    float const intensity = _submittedSpotlights[i].spotlight.intensity;
    lightData.spotlights[i].params.x = intensity;
    lightData.spotlights[i].params.y =
        std::cos(_submittedSpotlights[i].spotlight.cutoffAngleRad);
    lightData.spotlights[i].params.z =
        std::cos(_submittedSpotlights[i].spotlight.fadeoutAngleRad);
    lightData.spotlights[i].attenuation.x =
        _submittedSpotlights[i].spotlight.linearAttenuation;
    lightData.spotlights[i].attenuation.y =
        _submittedSpotlights[i].spotlight.quadraticAttenuation;
    lightData.spotlightShadowMapIndices[i].value =
        _submittedSpotlights[i].assignedShadowMapInd;
  }

  lightData.spotlightCount = _submittedSpotlights.size();

  return lightData;
}

void VulkanRHI::applyPendingExtentUpdate() {
  if (_pendingExtentUpdate) {
    vkDeviceWaitIdle(_vkDevice);
    _skipFrame = true;
    _vkbSwapchain.extent.width = _pendingExtentUpdate->width;
    _vkbSwapchain.extent.height = _pendingExtentUpdate->height;

    _swapchainBoundDescriptorAllocator.resetPools();
    _swapchainDeletionQueue.flush();

    initSwapchain(*_pendingExtentUpdate);
    initMainRenderPass();
    initSwapchainFramebuffers();
    initDepthPrepassFramebuffers();
    initSsaoFramebuffers();
    initSsaoPostProcessingFramebuffers();
    initDescriptors();
    initDepthPrepassDescriptors();
    initSsaoDescriptors();
    initSsaoPostProcessingDescriptors();

    _pendingExtentUpdate = std::nullopt;
  }
}
