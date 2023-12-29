#include <algorithm>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/utils/aabb.hpp>
#include <obsidian/core/utils/visitor.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/task/task_type.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_deletion_queue.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
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

using namespace obsidian;
using namespace obsidian::vk_rhi;

void VulkanRHI::waitDeviceIdle() const {
  std::scoped_lock l{_gpuQueueMutexes.at(_graphicsQueueFamilyIndex),
                     _gpuQueueMutexes.at(_transferQueueFamilyIndex)};

  vkDeviceWaitIdle(_vkDevice);
}

rhi::ResourceRHI& VulkanRHI::initTextureResource() {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Texture& newTexture = _textures[newResourceId];

  assert(newTexture.resource.state == rhi::ResourceState::initial);

  newTexture.resource.id = newResourceId;
  newTexture.resource.refCount = 1;
  return newTexture.resource;
}

void VulkanRHI::uploadTexture(rhi::ResourceIdRHI id,
                              rhi::UploadTextureRHI uploadTextureInfoRHI) {
  Texture& newTexture = _textures.at(id);

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!newTexture.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a texture that is not in the initial state.");
    return;
  }

  VkImageUsageFlags const imageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkExtent3D extent = {};
  extent.width = uploadTextureInfoRHI.width;
  extent.height = uploadTextureInfoRHI.height;
  extent.depth = 1;
  VkFormat const format = getVkTextureFormat(uploadTextureInfoRHI.format);

  VkImageCreateInfo vkImgCreateInfo = vkinit::imageCreateInfo(
      imageUsageFlags, extent, format, uploadTextureInfoRHI.mipLevels);

  VmaAllocationCreateInfo imgAllocationCreateInfo = {};
  imgAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateImage(_vmaAllocator, &vkImgCreateInfo,
                          &imgAllocationCreateInfo, &newTexture.image.vkImage,
                          &newTexture.image.allocation, nullptr));

  VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageViewCreateInfo(
      newTexture.image.vkImage, getVkTextureFormat(uploadTextureInfoRHI.format),
      VK_IMAGE_ASPECT_COLOR_BIT, uploadTextureInfoRHI.mipLevels);

  VK_CHECK(vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr,
                             &newTexture.imageView));

  assert(_taskExecutor);

  _taskExecutor->enqueue(
      task::TaskType::rhiTransfer,
      [this, &newTexture, extent, info = std::move(uploadTextureInfoRHI)]() {
        bool const hasMips = info.mipLevels > 1;

        std::size_t const size = info.width * info.height *
                                 core::getFormatPixelSize(info.format) *
                                 (hasMips ? 2 : 1);

        AllocatedBuffer stagingBuffer =
            createBuffer(size, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                         0, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

        void* mappedMemory;
        vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

        {
          ZoneScopedN("Upload Texture");
          info.unpackFunc(reinterpret_cast<char*>(mappedMemory));
        }

        vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

        immediateSubmit(_transferQueueFamilyIndex, [&newTexture, &info](
                                                       VkCommandBuffer cmd) {
          VkImageMemoryBarrier vkImgBarrierToTransfer =
              vkinit::layoutImageBarrier(
                  newTexture.image.vkImage, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_ASPECT_COLOR_BIT, info.mipLevels);
          vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
          vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

          vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                               nullptr, 1, &vkImgBarrierToTransfer);
        });

        immediateSubmit(_transferQueueFamilyIndex, [&extent, &newTexture,
                                                    &stagingBuffer, &info,
                                                    size](VkCommandBuffer cmd) {
          VkDeviceSize offset = 0;
          VkDeviceSize const mipLevelSize =
              info.mipLevels > 1 ? (size / 2) : size;

          for (std::size_t i = 0; i < info.mipLevels; ++i) {
            VkBufferImageCopy vkBufferImgCopy = {};
            vkBufferImgCopy.bufferOffset = offset;
            vkBufferImgCopy.imageExtent = {extent.width >> i,
                                           extent.height >> i, extent.depth};
            vkBufferImgCopy.imageSubresource.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            vkBufferImgCopy.imageSubresource.layerCount = 1;
            vkBufferImgCopy.imageSubresource.mipLevel = i;

            vkCmdCopyBufferToImage(
                cmd, stagingBuffer.buffer, newTexture.image.vkImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBufferImgCopy);

            offset += mipLevelSize >> (i * 2);
          }
        });

        vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                         stagingBuffer.allocation);

        immediateSubmit(_graphicsQueueFamilyIndex, [&newTexture, &info](
                                                       VkCommandBuffer cmd) {
          VkImageMemoryBarrier vkImageBarrierToRead =
              vkinit::layoutImageBarrier(
                  newTexture.image.vkImage,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_ASPECT_COLOR_BIT, info.mipLevels);

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
}

void VulkanRHI::releaseTexture(rhi::ResourceIdRHI resourceIdRHI) {
  Texture& tex = _textures[resourceIdRHI];
  --tex.resource.refCount;
}

rhi::ResourceRHI& VulkanRHI::initMeshResource() {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Mesh& newMesh = _meshes[newResourceId];

  assert(newMesh.resource.state == rhi::ResourceState::initial);

  newMesh.resource.id = newResourceId;
  newMesh.resource.refCount = 1;

  return newMesh.resource;
}

void VulkanRHI::uploadMesh(rhi::ResourceIdRHI id, rhi::UploadMeshRHI meshInfo) {
  Mesh& mesh = _meshes[id];

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!mesh.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a mesh that is not in the initial state.");
    return;
  }

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
  mesh.hasNormals = meshInfo.hasNormals;
  mesh.hasColors = meshInfo.hasColors;
  mesh.hasUV = meshInfo.hasUV;
  mesh.hasTangents = meshInfo.hasTangents;
  mesh.aabb = meshInfo.aabb;

  _taskExecutor->enqueue(
      task::TaskType::rhiTransfer,
      [this, totalIndexBufferSize, &mesh, info = std::move(meshInfo)]() {
        AllocatedBuffer stagingBuffer = createBuffer(
            info.vertexBufferSize + totalIndexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

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
}

void VulkanRHI::releaseMesh(rhi::ResourceIdRHI resourceIdRHI) {
  Mesh& mesh = _meshes[resourceIdRHI];
  --mesh.resource.refCount;
}

rhi::ResourceRHI& VulkanRHI::initShaderResource() {
  rhi::ResourceIdRHI newResourceId = consumeNewResourceId();
  Shader& shader = _shaderModules[newResourceId];

  assert(shader.resource.state == rhi::ResourceState::initial);

  shader.resource.id = newResourceId;
  shader.resource.refCount = 1;

  return shader.resource;
}

void VulkanRHI::uploadShader(rhi::ResourceIdRHI id,
                             rhi::UploadShaderRHI uploadShader) {
  Shader& shader = _shaderModules[id];

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!shader.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a shader that is not in the initial state.");
    return;
  }

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
    assert(false && "Failed to load shader.");
  }

  shader.vkShaderModule = shaderModule;

  expected = rhi::ResourceState::uploading;

  if (!shader.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploaded)) {
    assert(false && "Shader resource in invalid state");
  }
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

rhi::ResourceRHI& VulkanRHI::initMaterialResource() {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  VkMaterial& newMaterial = _materials[newResourceId];

  assert(newMaterial.resource.state == rhi::ResourceState::initial);

  newMaterial.resource.id = newResourceId;
  newMaterial.resource.refCount = 1;

  return newMaterial.resource;
}

void VulkanRHI::uploadMaterial(rhi::ResourceIdRHI id,
                               rhi::UploadMaterialRHI uploadMaterial) {
  VkMaterial& newMaterial = _materials[id];

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!newMaterial.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a shader that is not in the initial state.");
    return;
  }

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

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, false);
  newMaterial.vkPipelineReuseDepth = pipelineBuilder.buildPipeline(
      _vkDevice, _mainRenderPassReuseDepth.vkRenderPass);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, true);
  newMaterial.vkPipelineNoDepthReuse = pipelineBuilder.buildPipeline(
      _vkDevice, _mainRenderPassNoDepthReuse.vkRenderPass);

  newMaterial.transparent = uploadMaterial.transparent;
  newMaterial.reflection = uploadMaterial.reflection;
  newMaterial.refractionIndex = uploadMaterial.refractionIndex;

  DescriptorBuilder builder = DescriptorBuilder::begin(
      _vkDevice, _descriptorAllocator, _descriptorLayoutCache);

  VkDescriptorBufferInfo materialDataBufferInfo;
  VkDescriptorImageInfo diffuseTexImageInfo;
  VkDescriptorImageInfo normalMapTexImageInfo;

  if (uploadMaterial.materialType == core::MaterialType::lit) {
    GPULitMaterialData materialData;
    materialData.hasDiffuseTex =
        uploadMaterial.diffuseTextureId != rhi::rhiIdUninitialized;
    materialData.hasNormalMap =
        uploadMaterial.normalTextureId != rhi::rhiIdUninitialized;
    materialData.reflection = uploadMaterial.reflection;
    materialData.refractionIndex = uploadMaterial.refractionIndex;
    materialData.ambientColor = uploadMaterial.ambientColor;
    materialData.diffuseColor = uploadMaterial.diffuseColor;
    materialData.specularColor = uploadMaterial.specularColor;
    materialData.shininess = uploadMaterial.shininess;

    createAndBindMaterialDataBuffer(materialData, builder,
                                    materialDataBufferInfo);

  } else {
    GPUUnlitMaterialData materialData;
    materialData.hasDiffuseTex =
        uploadMaterial.diffuseTextureId != rhi::rhiIdUninitialized;
    materialData.diffuseColor = uploadMaterial.diffuseColor;

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

  if (uploadMaterial.hasTimer) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = _timerBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    builder.bindBuffer(3, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
  } else {
    builder.declareUnusedBuffer(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  builder.build(newMaterial.vkDescriptorSet);

  expected = rhi::ResourceState::uploading;

  if (!newMaterial.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploaded)) {
    assert(false && "Material resource in invalid state");
  }
}

void VulkanRHI::releaseMaterial(rhi::ResourceIdRHI resourceIdRHI) {
  VkMaterial& material = _materials[resourceIdRHI];
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
      vkDestroyPipeline(_vkDevice, mat.second.vkPipelineReuseDepth, nullptr);
      vkDestroyPipeline(_vkDevice, mat.second.vkPipelineNoDepthReuse, nullptr);
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
  Mesh& mesh = _meshes[drawCall.meshId];

  VKDrawCall vkDrawCall;
  vkDrawCall.model = drawCall.transform;
  vkDrawCall.mesh = &mesh;
  vkDrawCall.objectResourcesId = drawCall.objectResourcesId;

  for (std::size_t i = 0; i < drawCall.materialIds.size(); ++i) {
    vkDrawCall.material = &_materials[drawCall.materialIds[i]];
    vkDrawCall.indexBufferInd = i;
    if (vkDrawCall.material->transparent) {
      _transparentDrawCallQueue.push_back(vkDrawCall);
    } else {
      _drawCallQueue.push_back(vkDrawCall);

      if (vkDrawCall.mesh->hasTangents) {
        _ssaoDrawCallQueue.push_back(vkDrawCall);
      }
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
  ImmediateSubmitContext& immediateSubmitContext =
      getImmediateCtxForCurrentThread(queueInd);

  VkCommandBufferBeginInfo commandBufferBeginInfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(immediateSubmitContext.vkCommandBuffer,
                                &commandBufferBeginInfo));

  function(immediateSubmitContext.vkCommandBuffer);

  VK_CHECK(vkEndCommandBuffer(immediateSubmitContext.vkCommandBuffer));

  VkSubmitInfo submit =
      vkinit::commandBufferSubmitInfo(&immediateSubmitContext.vkCommandBuffer);

  {
    std::scoped_lock l{_gpuQueueMutexes.at(queueInd)};
    VK_CHECK(vkQueueSubmit(_gpuQueues[queueInd], 1, &submit,
                           immediateSubmitContext.vkFence));
  }

  vkWaitForFences(_vkDevice, 1, &immediateSubmitContext.vkFence, VK_TRUE,
                  9999999999);
  vkResetFences(_vkDevice, 1, &immediateSubmitContext.vkFence);

  VK_CHECK(
      vkResetCommandPool(_vkDevice, immediateSubmitContext.vkCommandPool, 0));
}

FrameData& VulkanRHI::getCurrentFrameData() {
  std::size_t const currentFrameDataInd = _frameNumber % frameOverlap;
  return _frameDataArray[currentFrameDataInd];
}

AllocatedBuffer VulkanRHI::createBuffer(
    std::size_t bufferSize, VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocationCreateFlags,
    VkMemoryPropertyFlags preferredFlags, VkMemoryPropertyFlags requiredFlags,
    VmaAllocationInfo* outAllocationInfo) const {
  VkBufferCreateInfo vkBufferCreateInfo = {};
  vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vkBufferCreateInfo.pNext = nullptr;
  vkBufferCreateInfo.size = bufferSize;
  vkBufferCreateInfo.usage = usage;

  VmaAllocationCreateInfo vmaAllocationCreateInfo = {};
  vmaAllocationCreateInfo.usage = memoryUsage;
  vmaAllocationCreateInfo.flags = allocationCreateFlags;
  vmaAllocationCreateInfo.preferredFlags = preferredFlags;
  vmaAllocationCreateInfo.requiredFlags = requiredFlags;

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
    waitDeviceIdle();
    _skipFrame = true;
    _vkbSwapchain.extent.width = _pendingExtentUpdate->width;
    _vkbSwapchain.extent.height = _pendingExtentUpdate->height;

    _swapchainBoundDescriptorAllocator.resetPools();
    _swapchainDeletionQueue.flush();

    initSwapchain(*_pendingExtentUpdate);
    initMainRenderPasses();
    initDepthPrepassFramebuffers();
    initSwapchainFramebuffers();
    initSsaoFramebuffers();
    initSsaoPostProcessingFramebuffers();
    initDescriptors();
    initDepthPrepassDescriptors();
    initSsaoDescriptors();
    initSsaoPostProcessingDescriptors();

    _pendingExtentUpdate = std::nullopt;
  }
}

static thread_local std::unordered_map<std::uint32_t, ImmediateSubmitContext>
    immediateSubmitContext;
static thread_local bool contextsInitialized = false;

ImmediateSubmitContext&
VulkanRHI::getImmediateCtxForCurrentThread(std::uint32_t queueIdx) {
  if (!contextsInitialized) {
    for (auto const& q : _gpuQueues) {
      initImmediateSubmitContext(immediateSubmitContext[q.first], q.first);
    }

    contextsInitialized = true;
  }

  return immediateSubmitContext[queueIdx];
}

// used on main thread to manually destroy the context
void VulkanRHI::destroyImmediateCtxForCurrentThread() {
  immediateSubmitContext = {};
  contextsInitialized = false;
}
