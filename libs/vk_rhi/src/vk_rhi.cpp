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
#include <obsidian/vk_rhi/vk_debug.hpp>
#include <obsidian/vk_rhi/vk_deletion_queue.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_frame_data.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>

#include <algorithm>
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

  VK_CHECK(vkDeviceWaitIdle(_vkDevice));
}

rhi::ResourceRHI& VulkanRHI::initTextureResource() {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Texture& newTexture = _textures[newResourceId];

  assert(newTexture.resource.state == rhi::ResourceState::initial);

  newTexture.resource.id = newResourceId;
  newTexture.resource.refCount = 1;
  return newTexture.resource;
}

rhi::ResourceTransferRHI
VulkanRHI::uploadTexture(rhi::ResourceIdRHI id,
                         rhi::UploadTextureRHI uploadTextureInfoRHI) {
  Texture& newTexture = _textures.at(id);

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!newTexture.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a texture that is not in the initial state.");
    return {};
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

  setDbgResourceName(_vkDevice, (std::uint64_t)newTexture.image.vkImage,
                     VK_OBJECT_TYPE_IMAGE, uploadTextureInfoRHI.debugName);

  VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageViewCreateInfo(
      newTexture.image.vkImage, getVkTextureFormat(uploadTextureInfoRHI.format),
      VK_IMAGE_ASPECT_COLOR_BIT, uploadTextureInfoRHI.mipLevels);

  VK_CHECK(vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr,
                             &newTexture.imageView));

  setDbgResourceName(_vkDevice, (std::uint64_t)newTexture.imageView,
                     VK_OBJECT_TYPE_IMAGE_VIEW, uploadTextureInfoRHI.debugName);

  return rhi::ResourceTransferRHI{_taskExecutor.enqueue(
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
        vmaFlushAllocation(_vmaAllocator, stagingBuffer.allocation, 0, size);

        ImageTransferInfo const transferInfo = {
            .format = info.format,
            .width = info.width,
            .height = info.height,
            .mipCount = info.mipLevels,
            .layerCount = 1,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        };

        ImageTransferDstState const transferDstState = {
            _graphicsQueueFamilyIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

        transferDataToImage(stagingBuffer, newTexture.image.vkImage,
                            transferInfo, VK_QUEUE_FAMILY_IGNORED,
                            transferDstState);

        rhi::ResourceState expected = rhi::ResourceState::uploading;

        if (!newTexture.resource.state.compare_exchange_strong(
                expected, rhi::ResourceState::uploaded)) {
          OBS_LOG_ERR("Texture resource state expected to be uploading.");
        }
      })};
}

void VulkanRHI::releaseTexture(rhi::ResourceIdRHI resourceIdRHI) {
  Texture& tex = _textures[resourceIdRHI];
  if (!--tex.resource.refCount) {
    FrameData& prevFrameData = getPreviousFrameData();
    prevFrameData.pendingResourcesToDestroy.texturesToDestroy.push_back(
        resourceIdRHI);
  }
}

rhi::ResourceRHI& VulkanRHI::initMeshResource() {
  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Mesh& newMesh = _meshes[newResourceId];

  assert(newMesh.resource.state == rhi::ResourceState::initial);

  newMesh.resource.id = newResourceId;
  newMesh.resource.refCount = 1;

  return newMesh.resource;
}

rhi::ResourceTransferRHI VulkanRHI::uploadMesh(rhi::ResourceIdRHI id,
                                               rhi::UploadMeshRHI meshInfo) {
  Mesh& mesh = _meshes[id];

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!mesh.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a mesh that is not in the initial state.");
    return {};
  }

  mesh.vertexBuffer = createBuffer(meshInfo.vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  mesh.vertexCount = meshInfo.vertexCount;

  setDbgResourceName(_vkDevice, (std::uint64_t)mesh.vertexBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, meshInfo.debugName,
                     "Vertex Buffer");

  std::size_t const totalIndexBufferSize =
      std::accumulate(meshInfo.indexBufferSizes.cbegin(),
                      meshInfo.indexBufferSizes.cend(), std::size_t{0});

  mesh.indexBuffer = createBuffer(totalIndexBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)mesh.indexBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, meshInfo.debugName, "Index Buffer");

  mesh.indexBufferSizes = meshInfo.indexBufferSizes;
  mesh.indexCount = meshInfo.indexCount;
  mesh.hasNormals = meshInfo.hasNormals;
  mesh.hasColors = meshInfo.hasColors;
  mesh.hasUV = meshInfo.hasUV;
  mesh.hasTangents = meshInfo.hasTangents;
  mesh.aabb = meshInfo.aabb;

  return rhi::ResourceTransferRHI{_taskExecutor.enqueue(
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
      })};
}

void VulkanRHI::releaseMesh(rhi::ResourceIdRHI resourceIdRHI) {
  Mesh& mesh = _meshes[resourceIdRHI];
  if (!--mesh.resource.refCount) {
    FrameData& prevFrameData = getPreviousFrameData();
    prevFrameData.pendingResourcesToDestroy.meshesToDestroy.push_back(
        resourceIdRHI);
  }
}

rhi::ResourceRHI& VulkanRHI::initShaderResource() {
  rhi::ResourceIdRHI newResourceId = consumeNewResourceId();
  Shader& shader = _shaderModules[newResourceId];

  assert(shader.resource.state == rhi::ResourceState::initial);

  shader.resource.id = newResourceId;
  shader.resource.refCount = 1;

  return shader.resource;
}

rhi::ResourceTransferRHI
VulkanRHI::uploadShader(rhi::ResourceIdRHI id,
                        rhi::UploadShaderRHI uploadShader) {
  Shader& shader = _shaderModules.at(id);

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!shader.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a shader that is not in the initial state.");
    return {};
  }

  return rhi::ResourceTransferRHI{_taskExecutor.enqueue(
      task::TaskType::rhiTransfer,
      [this, id, uploadShader = std::move(uploadShader)]() {
        Shader& shader = _shaderModules.at(id);

        std::vector<std::uint32_t> buffer(
            (uploadShader.shaderDataSize + sizeof(std::uint32_t) - 1) /
            sizeof(std::uint32_t));

        {
          ZoneScopedN("Unpack Shader");
          uploadShader.unpackFunc(reinterpret_cast<char*>(buffer.data()));
        }

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
        shaderModuleCreateInfo.sType =
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.codeSize = uploadShader.shaderDataSize;
        shaderModuleCreateInfo.pCode = buffer.data();

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(_vkDevice, &shaderModuleCreateInfo, nullptr,
                                 &shaderModule)) {
          assert(false && "Failed to load shader.");
        }

        setDbgResourceName(_vkDevice, (std::uint64_t)shaderModule,
                           VK_OBJECT_TYPE_SHADER_MODULE,
                           uploadShader.debugName);

        shader.vkShaderModule = shaderModule;

        rhi::ResourceState expected = rhi::ResourceState::uploading;

        if (!shader.resource.state.compare_exchange_strong(
                expected, rhi::ResourceState::uploaded)) {
          assert(false && "Shader resource in invalid state");
        }
      })};
}

void VulkanRHI::releaseShader(rhi::ResourceIdRHI resourceIdRHI) {
  Shader& shader = _shaderModules.at(resourceIdRHI);
  if (!--shader.resource.refCount) {
    FrameData& prevFrameData = getPreviousFrameData();
    prevFrameData.pendingResourcesToDestroy.shadersToDestroy.push_back(
        resourceIdRHI);
  }
}

template <typename MaterialDataT>
void VulkanRHI::createAndBindMaterialDataBuffer(
    MaterialDataT const& materialData, DescriptorBuilder& builder,
    VkDescriptorBufferInfo& outBufferInfo) {
  AllocatedBuffer materialDataBuffer = createBuffer(
      getPaddedBufferSize(sizeof(MaterialDataT)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  uploadBufferData(0, materialData, materialDataBuffer,
                   VK_QUEUE_FAMILY_IGNORED);

  _deletionQueue.pushFunction([this, materialDataBuffer]() {
    vmaDestroyBuffer(_vmaAllocator, materialDataBuffer.buffer,
                     materialDataBuffer.allocation);
  });

  outBufferInfo.buffer = materialDataBuffer.buffer;
  outBufferInfo.offset = 0;
  outBufferInfo.range = sizeof(MaterialDataT);

  builder.bindBuffer(0, outBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
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

rhi::ResourceTransferRHI
VulkanRHI::uploadMaterial(rhi::ResourceIdRHI id,
                          rhi::UploadMaterialRHI uploadMaterial) {
  VkMaterial& newMaterial = _materials[id];

  rhi::ResourceState expected = rhi::ResourceState::initial;

  if (!newMaterial.resource.state.compare_exchange_strong(
          expected, rhi::ResourceState::uploading)) {
    OBS_LOG_ERR("Trying to upload a shader that is not in the initial state.");
    return {};
  }

  return rhi::ResourceTransferRHI{_taskExecutor.enqueue(
      task::TaskType::rhiTransfer,
      [this, id, uploadMaterial = std::move(uploadMaterial)]() {
        VkMaterial& newMaterial = _materials[id];

        PipelineBuilder pipelineBuilder =
            _pipelineBuilders.at(uploadMaterial.materialType);

        Shader& vertexShaderModule =
            _shaderModules.at(uploadMaterial.vertexShaderId);
        ++vertexShaderModule.resource.refCount;
        newMaterial.vertexShaderResourceDependencyId =
            vertexShaderModule.resource.id;

        pipelineBuilder._vkShaderStageCreateInfos.clear();
        pipelineBuilder._vkShaderStageCreateInfos.push_back(
            vkinit::pipelineShaderStageCreateInfo(
                VK_SHADER_STAGE_VERTEX_BIT, vertexShaderModule.vkShaderModule));

        Shader& fragmentShaderModule =
            _shaderModules.at(uploadMaterial.fragmentShaderId);
        ++fragmentShaderModule.resource.refCount;
        newMaterial.fragmentShaderResourceDependencyId =
            fragmentShaderModule.resource.id;

        pipelineBuilder._vkShaderStageCreateInfos.push_back(
            vkinit::pipelineShaderStageCreateInfo(
                VK_SHADER_STAGE_FRAGMENT_BIT,
                fragmentShaderModule.vkShaderModule));

        newMaterial.vkPipelineLayout = pipelineBuilder._vkPipelineLayout;

        pipelineBuilder._vkDepthStencilStateCreateInfo =
            vkinit::depthStencilStateCreateInfo(true, false);
        newMaterial.vkPipelineReuseDepth = pipelineBuilder.buildPipeline(
            _vkDevice, _mainRenderPassReuseDepth.vkRenderPass);

        setDbgResourceName(_vkDevice,
                           (std::uint64_t)newMaterial.vkPipelineReuseDepth,
                           VK_OBJECT_TYPE_PIPELINE, uploadMaterial.debugName,
                           "Reuse depth pipeline");

        pipelineBuilder._vkDepthStencilStateCreateInfo =
            vkinit::depthStencilStateCreateInfo(true, true);
        pipelineBuilder._vkRasterizationCreateInfo.frontFace =
            VK_FRONT_FACE_CLOCKWISE;
        newMaterial.vkPipelineEnvironmentRendering =
            pipelineBuilder.buildPipeline(_vkDevice,
                                          _envMapRenderPass.vkRenderPass);
        pipelineBuilder._vkRasterizationCreateInfo.frontFace =
            VK_FRONT_FACE_COUNTER_CLOCKWISE;

        setDbgResourceName(
            _vkDevice,
            (std::uint64_t)newMaterial.vkPipelineEnvironmentRendering,
            VK_OBJECT_TYPE_PIPELINE, uploadMaterial.debugName,
            "Environment rendering pipeline");

        newMaterial.transparent = uploadMaterial.transparent;

        DescriptorBuilder descriptorBuilder = DescriptorBuilder::begin(
            _vkDevice, _descriptorAllocator, _descriptorLayoutCache);

        VkDescriptorBufferInfo materialDataBufferInfo;

        if (uploadMaterial.materialType == core::MaterialType::lit) {
          rhi::UploadLitMaterialRHI const& uploadLitMaterial =
              std::get<rhi::UploadLitMaterialRHI>(
                  uploadMaterial.uploadMaterialSubtype);

          newMaterial.reflection = uploadLitMaterial.reflection;

          GPULitMaterialData materialData;
          materialData.hasDiffuseTex =
              uploadLitMaterial.diffuseTextureId != rhi::rhiIdUninitialized;
          materialData.hasNormalMap =
              uploadLitMaterial.normalTextureId != rhi::rhiIdUninitialized;
          materialData.reflection = uploadLitMaterial.reflection;
          materialData.ambientColor = uploadLitMaterial.ambientColor;
          materialData.diffuseColor = uploadLitMaterial.diffuseColor;
          materialData.specularColor = uploadLitMaterial.specularColor;
          materialData.shininess = uploadLitMaterial.shininess;

          createAndBindMaterialDataBuffer(materialData, descriptorBuilder,
                                          materialDataBufferInfo);

          bool const hasDiffuseTex =
              uploadLitMaterial.diffuseTextureId != rhi::rhiIdUninitialized;

          if (hasDiffuseTex) {
            Texture& diffuseTexture =
                _textures[uploadLitMaterial.diffuseTextureId];

            ++diffuseTexture.resource.refCount;
            newMaterial.textureResourceDependencyIds.push_back(
                diffuseTexture.resource.id);

            VkDescriptorImageInfo diffuseTexImageInfo;
            diffuseTexImageInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            diffuseTexImageInfo.imageView = diffuseTexture.imageView;
            diffuseTexImageInfo.sampler = _vkLinearRepeatSampler;

            descriptorBuilder.bindImage(
                1, diffuseTexImageInfo,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
          } else {
            descriptorBuilder.declareUnusedImage(
                1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);
          }

          bool const hasNormalMap =
              uploadLitMaterial.normalTextureId != rhi::rhiIdUninitialized;

          if (hasNormalMap) {
            VkDescriptorImageInfo normalMapTexImageInfo;
            normalMapTexImageInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalMapTexImageInfo.sampler = _vkLinearRepeatSampler;
            Texture& normalMapTexture =
                _textures[uploadLitMaterial.normalTextureId];

            ++normalMapTexture.resource.refCount;
            newMaterial.textureResourceDependencyIds.push_back(
                normalMapTexture.resource.id);

            normalMapTexImageInfo.imageView = normalMapTexture.imageView;

            descriptorBuilder.bindImage(
                2, normalMapTexImageInfo,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
          } else {
            descriptorBuilder.declareUnusedImage(
                2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);
          }
        } else if (uploadMaterial.materialType == core::MaterialType::unlit) {
          rhi::UploadUnlitMaterialRHI const& uploadUnlitMaterial =
              std::get<rhi::UploadUnlitMaterialRHI>(
                  uploadMaterial.uploadMaterialSubtype);
          GPUUnlitMaterialData materialData;
          materialData.hasColorTex =
              uploadUnlitMaterial.colorTextureId != rhi::rhiIdUninitialized;
          materialData.color = uploadUnlitMaterial.color;

          createAndBindMaterialDataBuffer(materialData, descriptorBuilder,
                                          materialDataBufferInfo);

          bool const hasColorTex =
              uploadUnlitMaterial.colorTextureId != rhi::rhiIdUninitialized;
          if (hasColorTex) {
            Texture& colorTexture =
                _textures[uploadUnlitMaterial.colorTextureId];

            ++colorTexture.resource.refCount;
            newMaterial.textureResourceDependencyIds.push_back(
                colorTexture.resource.id);

            VkDescriptorImageInfo colorTexImageInfo;
            colorTexImageInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            colorTexImageInfo.imageView = colorTexture.imageView;
            colorTexImageInfo.sampler = _vkLinearRepeatSampler;

            descriptorBuilder.bindImage(
                1, colorTexImageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
          } else {
            descriptorBuilder.declareUnusedImage(
                1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);
          }
        } else if (uploadMaterial.materialType == core::MaterialType::pbr) {
          rhi::UploadPBRMaterialRHI const& uploadPbrMaterial =
              std::get<rhi::UploadPBRMaterialRHI>(
                  uploadMaterial.uploadMaterialSubtype);

          GPUPbrMaterialData pbrMaterialData;
          pbrMaterialData.metalnessAndRoughnessSeparate =
              uploadPbrMaterial.roughnessTextureId != rhi::rhiIdUninitialized;

          createAndBindMaterialDataBuffer(pbrMaterialData, descriptorBuilder,
                                          materialDataBufferInfo);

          Texture& albedoTexture = _textures[uploadPbrMaterial.albedoTextureId];

          ++albedoTexture.resource.refCount;
          newMaterial.textureResourceDependencyIds.push_back(
              albedoTexture.resource.id);

          VkDescriptorImageInfo albedoTexImageInfo;
          albedoTexImageInfo.imageLayout =
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          albedoTexImageInfo.imageView = albedoTexture.imageView;
          albedoTexImageInfo.sampler = _vkLinearRepeatSampler;

          descriptorBuilder.bindImage(1, albedoTexImageInfo,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

          Texture& normalTexture = _textures[uploadPbrMaterial.normalTextureId];

          ++normalTexture.resource.refCount;
          newMaterial.textureResourceDependencyIds.push_back(
              normalTexture.resource.id);

          VkDescriptorImageInfo normalTexImageInfo;
          normalTexImageInfo.imageLayout =
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          normalTexImageInfo.imageView = normalTexture.imageView;
          normalTexImageInfo.sampler = _vkLinearRepeatSampler;

          descriptorBuilder.bindImage(2, normalTexImageInfo,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

          Texture& metalnessTexture =
              _textures[uploadPbrMaterial.metalnessTextureId];

          ++metalnessTexture.resource.refCount;
          newMaterial.textureResourceDependencyIds.push_back(
              metalnessTexture.resource.id);

          VkDescriptorImageInfo metalnessTexImageInfo;
          metalnessTexImageInfo.imageLayout =
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          metalnessTexImageInfo.imageView = metalnessTexture.imageView;
          metalnessTexImageInfo.sampler = _vkLinearRepeatSampler;

          descriptorBuilder.bindImage(3, metalnessTexImageInfo,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

          if (pbrMaterialData.metalnessAndRoughnessSeparate) {
            Texture& roughnessTexture =
                _textures[uploadPbrMaterial.roughnessTextureId];

            ++roughnessTexture.resource.refCount;
            newMaterial.textureResourceDependencyIds.push_back(
                roughnessTexture.resource.id);

            VkDescriptorImageInfo roughnessTexImageInfo;
            roughnessTexImageInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            roughnessTexImageInfo.imageView = metalnessTexture.imageView;
            roughnessTexImageInfo.sampler = _vkLinearRepeatSampler;

            descriptorBuilder.bindImage(
                4, roughnessTexImageInfo,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
          } else {
            descriptorBuilder.declareUnusedImage(
                4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);
          }
        }

        if (uploadMaterial.hasTimer) {
          VkDescriptorBufferInfo bufferInfo = {};
          bufferInfo.buffer = _timerBuffer.buffer;
          bufferInfo.offset = 0;
          bufferInfo.range = VK_WHOLE_SIZE;
          descriptorBuilder.bindBuffer(
              10, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
        } else {
          descriptorBuilder.declareUnusedBuffer(
              10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        if (!descriptorBuilder.build(newMaterial.vkDescriptorSet)) {
          OBS_LOG_ERR("Failed to build descriptor set when uploading material");
          newMaterial.resource.state = rhi::ResourceState::invalid;
        } else {
          rhi::ResourceState expected = rhi::ResourceState::uploading;

          if (!newMaterial.resource.state.compare_exchange_strong(
                  expected, rhi::ResourceState::uploaded)) {
            assert(false && "Material resource in invalid state");
          }
        }
      })};
}

void VulkanRHI::releaseMaterial(rhi::ResourceIdRHI resourceIdRHI) {
  VkMaterial& material = _materials[resourceIdRHI];

  if (!--material.resource.refCount) {
    FrameData& prevFrameData = getPreviousFrameData();
    prevFrameData.pendingResourcesToDestroy.materialsToDestroy.push_back(
        resourceIdRHI);

    Shader& vertexShaderDependency =
        _shaderModules.at(material.vertexShaderResourceDependencyId);

    if (!--vertexShaderDependency.resource.refCount) {
      prevFrameData.pendingResourcesToDestroy.shadersToDestroy.push_back(
          material.vertexShaderResourceDependencyId);
    }

    Shader& fragmentShaderDependency =
        _shaderModules.at(material.fragmentShaderResourceDependencyId);

    if (!--fragmentShaderDependency.resource.refCount) {
      prevFrameData.pendingResourcesToDestroy.shadersToDestroy.push_back(
          material.fragmentShaderResourceDependencyId);
    }

    for (rhi::ResourceIdRHI textureId : material.textureResourceDependencyIds) {
      Texture& textureDependency = _textures.at(textureId);

      if (!--textureDependency.resource.refCount) {
        prevFrameData.pendingResourcesToDestroy.texturesToDestroy.push_back(
            textureId);
      }
    }
  }
}

void VulkanRHI::destroyUnusedResources(
    PendingResourcesToDestroy& pendingResourcesToDestroy) {
  // clear materials
  for (rhi::ResourceIdRHI matId :
       pendingResourcesToDestroy.materialsToDestroy) {
    VkMaterial& mat = _materials.at(matId);

    if (mat.vkPipelineReuseDepth) {
      vkDestroyPipeline(_vkDevice, mat.vkPipelineReuseDepth, nullptr);
    }
    if (mat.vkPipelineEnvironmentRendering) {
      vkDestroyPipeline(_vkDevice, mat.vkPipelineEnvironmentRendering, nullptr);
    }

    _materials.erase(matId);
  }

  pendingResourcesToDestroy.materialsToDestroy.clear();

  // clear textures
  for (rhi::ResourceIdRHI texId : pendingResourcesToDestroy.texturesToDestroy) {
    Texture& tex = _textures.at(texId);

    vkDestroyImageView(_vkDevice, tex.imageView, nullptr);
    vmaDestroyImage(_vmaAllocator, tex.image.vkImage, tex.image.allocation);

    _textures.erase(texId);
  }

  pendingResourcesToDestroy.texturesToDestroy.clear();

  // clear shaders
  for (rhi::ResourceIdRHI shaderId :
       pendingResourcesToDestroy.shadersToDestroy) {
    Shader& shader = _shaderModules.at(shaderId);
    vkDestroyShaderModule(_vkDevice, shader.vkShaderModule, nullptr);

    _shaderModules.erase(shaderId);
  }

  pendingResourcesToDestroy.shadersToDestroy.clear();

  // clear meshes
  for (rhi::ResourceIdRHI meshId : pendingResourcesToDestroy.meshesToDestroy) {
    Mesh& mesh = _meshes.at(meshId);

    vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);

    vmaDestroyBuffer(_vmaAllocator, mesh.indexBuffer.buffer,
                     mesh.indexBuffer.allocation);

    _meshes.erase(meshId);
  }

  pendingResourcesToDestroy.meshesToDestroy.clear();

  // clear environment maps

  for (rhi::ResourceIdRHI envMapId :
       pendingResourcesToDestroy.environmentMapsToDestroy) {
    assert(_environmentMaps.contains(envMapId));

    EnvironmentMap& envMap = _environmentMaps.at(envMapId);

    for (std::size_t i = 0; i < envMap.framebuffers.size(); ++i) {
      vkDestroyFramebuffer(_vkDevice, envMap.framebuffers[i], nullptr);
      vkDestroyImageView(_vkDevice, envMap.colorAttachmentImageViews[i],
                         nullptr);
      vkDestroyImageView(_vkDevice, envMap.depthAttachmentImageViews[i],
                         nullptr);
    }

    vkDestroyImageView(_vkDevice, envMap.colorImageView, nullptr);
    vmaDestroyImage(_vmaAllocator, envMap.colorImage.vkImage,
                    envMap.colorImage.allocation);
    vkDestroyImageView(_vkDevice, envMap.depthImageView, nullptr);
    vmaDestroyImage(_vmaAllocator, envMap.depthImage.vkImage,
                    envMap.depthImage.allocation);

    vmaDestroyBuffer(_vmaAllocator, envMap.cameraBuffer.buffer,
                     envMap.cameraBuffer.allocation);

    _environmentMaps.erase(envMapId);
  }

  pendingResourcesToDestroy.environmentMapsToDestroy.clear();

  {
    std::scoped_lock l{_resourceTransfersMutex};

    static std::vector<ResourceTransfer> transfers;

    _resourceTransfers.swap(transfers);

    for (auto const& t : transfers) {
      VkResult const result = vkGetFenceStatus(_vkDevice, t.transferFence);
      if (result == VK_SUCCESS) {
        vmaDestroyBuffer(_vmaAllocator, t.stagingBuffer.buffer,
                         t.stagingBuffer.allocation);

        vkDestroyFence(_vkDevice, t.transferFence, nullptr);

        for (VkSemaphore s : t.semaphores) {
          vkDestroySemaphore(_vkDevice, s, nullptr);
        }
      } else if (result == VK_NOT_READY) {
        _resourceTransfers.push_back(t);
      } else {
        VK_CHECK(result);
      }
    }

    transfers.clear();
  }
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

  {
    ZoneScopedN("ImmediateSubmit wait for fences");
    vkWaitForFences(_vkDevice, 1, &immediateSubmitContext.vkFence, VK_TRUE,
                    9999999999);
  }
  VK_CHECK(vkResetFences(_vkDevice, 1, &immediateSubmitContext.vkFence));

  VK_CHECK(
      vkResetCommandPool(_vkDevice, immediateSubmitContext.vkCommandPool, 0));
}

void VulkanRHI::initResourceTransferContext(ResourceTransferContext& ctx) {
  assert(!ctx.initialized);

  ctx.device = _vkDevice;

  for (std::uint32_t queueFamilyIdx : _queueFamilyIndices) {
    VkCommandPoolCreateInfo commandPoolCreateInfo =
        vkinit::commandPoolCreateInfo(queueFamilyIdx,
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    VkCommandPool& pool = ctx.queueCommandPools[queueFamilyIdx];
    vkCreateCommandPool(ctx.device, &commandPoolCreateInfo, nullptr, &pool);
    setDbgResourceName(_vkDevice, (std::uint64_t)pool,
                       VK_OBJECT_TYPE_COMMAND_POOL, "Resource transfer pool");
  }

  ctx.initialized = true;
}

ResourceTransferContext&
VulkanRHI::getResourceTransferContextForCurrentThread() {
  static thread_local ResourceTransferContext ctx;

  if (!ctx.initialized) {
    initResourceTransferContext(ctx);
  }

  return ctx;
}

void VulkanRHI::destroyResourceTransferContextForCurrentThread() {
  getResourceTransferContextForCurrentThread().cleanup();
}

void VulkanRHI::transferDataToImage(AllocatedBuffer stagingBuffer,
                                    VkImage dstImg,
                                    ImageTransferInfo const& imgTransferInfo,
                                    std::uint32_t currentImgQueueFamilyIdx,
                                    ImageTransferDstState transferDstState) {
  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  ResourceTransfer transfer = {};
  transfer.stagingBuffer = stagingBuffer;

  VkFenceCreateInfo fenceCreateInfo = {.sType =
                                           VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = 0};

  VK_CHECK(vkCreateFence(ctx.device, &fenceCreateInfo, nullptr,
                         &transfer.transferFence));

  VkSemaphore transitionToTransferQueueSemaphore = VK_NULL_HANDLE;

  VkCommandBuffer cmdTransfer = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo const cmdTransferAllocInfo =
      vkinit::commandBufferAllocateInfo(
          ctx.queueCommandPools.at(_transferQueueFamilyIndex));

  VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdTransferAllocInfo,
                                    &cmdTransfer));
  transfer.commandBuffers.push_back(cmdTransfer);

  VkCommandBufferBeginInfo const cmdTransferBegininfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  vkBeginCommandBuffer(cmdTransfer, &cmdTransferBegininfo);

  VkImageMemoryBarrier barrierTransitionToTransferQueue = {};
  barrierTransitionToTransferQueue.sType =
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrierTransitionToTransferQueue.pNext = nullptr;
  barrierTransitionToTransferQueue.srcAccessMask = VK_ACCESS_NONE;
  barrierTransitionToTransferQueue.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrierTransitionToTransferQueue.srcQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrierTransitionToTransferQueue.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrierTransitionToTransferQueue.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrierTransitionToTransferQueue.newLayout =
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrierTransitionToTransferQueue.image = dstImg;
  barrierTransitionToTransferQueue.subresourceRange.aspectMask =
      imgTransferInfo.aspectMask;
  barrierTransitionToTransferQueue.subresourceRange.baseMipLevel = 0;
  barrierTransitionToTransferQueue.subresourceRange.levelCount =
      VK_REMAINING_MIP_LEVELS;
  barrierTransitionToTransferQueue.subresourceRange.baseArrayLayer = 0;
  barrierTransitionToTransferQueue.subresourceRange.layerCount =
      imgTransferInfo.layerCount;

  if (currentImgQueueFamilyIdx != VK_QUEUE_FAMILY_IGNORED &&
      currentImgQueueFamilyIdx != _transferQueueFamilyIndex) {
    // Transition queue ownership from current queue to transfer queue
    barrierTransitionToTransferQueue.srcQueueFamilyIndex =
        currentImgQueueFamilyIdx;
    barrierTransitionToTransferQueue.dstQueueFamilyIndex =
        _transferQueueFamilyIndex;

    VkCommandBuffer cmdRelease = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo const cmdReleaseAllocInfo =
        vkinit::commandBufferAllocateInfo(
            ctx.queueCommandPools.at(currentImgQueueFamilyIdx));

    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdReleaseAllocInfo,
                                      &cmdRelease));
    transfer.commandBuffers.push_back(cmdRelease);

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToTransferQueueSemaphore));
    transfer.semaphores.push_back(transitionToTransferQueueSemaphore);

    VkCommandBufferBeginInfo cmdBufferBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkBeginCommandBuffer(cmdRelease, &cmdBufferBeginInfo);

    vkCmdPipelineBarrier(cmdRelease, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrierTransitionToTransferQueue);

    vkEndCommandBuffer(cmdRelease);

    VkSubmitInfo releaseSubmitInfo = {};
    releaseSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    releaseSubmitInfo.pNext = nullptr;

    releaseSubmitInfo.commandBufferCount = 1;
    releaseSubmitInfo.pCommandBuffers = &cmdRelease;
    releaseSubmitInfo.signalSemaphoreCount = 1;
    releaseSubmitInfo.pSignalSemaphores = &transitionToTransferQueueSemaphore;

    {
      std::scoped_lock l{_gpuQueueMutexes.at(currentImgQueueFamilyIdx)};
      VK_CHECK(vkQueueSubmit(_gpuQueues.at(currentImgQueueFamilyIdx), 1,
                             &releaseSubmitInfo, VK_NULL_HANDLE));
    }
  }

  vkCmdPipelineBarrier(cmdTransfer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrierTransitionToTransferQueue);

  std::size_t const size = imgTransferInfo.width * imgTransferInfo.height *
                           core::getFormatPixelSize(imgTransferInfo.format) *
                           (imgTransferInfo.mipCount > 1 ? 2 : 1);
  VkDeviceSize const mipLevelSize =
      imgTransferInfo.mipCount > 1 ? (size / 2) : size;

  VkDeviceSize offset = 0;
  for (std::size_t i = 0; i < imgTransferInfo.mipCount; ++i) {
    VkBufferImageCopy vkBufferImgCopy = {};
    vkBufferImgCopy.bufferOffset = offset;
    vkBufferImgCopy.imageExtent = {imgTransferInfo.width >> i,
                                   imgTransferInfo.height >> i, 1};
    vkBufferImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBufferImgCopy.imageSubresource.layerCount = 1;
    vkBufferImgCopy.imageSubresource.mipLevel = i;

    vkCmdCopyBufferToImage(cmdTransfer, stagingBuffer.buffer, dstImg,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &vkBufferImgCopy);

    offset += mipLevelSize >> (i * 2);
  }

  VkSubmitInfo transferSubmitInfo = {};
  transferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  transferSubmitInfo.pNext = nullptr;

  if (transitionToTransferQueueSemaphore != VK_NULL_HANDLE) {
    transferSubmitInfo.waitSemaphoreCount = 1;
    transferSubmitInfo.pWaitSemaphores = &transitionToTransferQueueSemaphore;
  }

  VkPipelineStageFlags waitDstStageFlag = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  transferSubmitInfo.pWaitDstStageMask = &waitDstStageFlag;
  transferSubmitInfo.commandBufferCount = 1;
  transferSubmitInfo.pCommandBuffers = &cmdTransfer;

  if (transferDstState.dstImgQueueFamilyIdx != VK_QUEUE_FAMILY_IGNORED &&
      transferDstState.dstImgQueueFamilyIdx != _transferQueueFamilyIndex) {
    // transition image ownership to dst queue family with barriers

    VkImageMemoryBarrier barrierTransitionToDstQueue = {};
    barrierTransitionToDstQueue.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierTransitionToDstQueue.pNext = nullptr;
    barrierTransitionToDstQueue.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierTransitionToDstQueue.dstAccessMask = VK_ACCESS_NONE;
    barrierTransitionToDstQueue.srcQueueFamilyIndex = _transferQueueFamilyIndex;
    barrierTransitionToDstQueue.dstQueueFamilyIndex =
        transferDstState.dstImgQueueFamilyIdx;
    barrierTransitionToDstQueue.oldLayout =
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierTransitionToDstQueue.newLayout = transferDstState.dstLayout;
    barrierTransitionToDstQueue.image = dstImg;
    barrierTransitionToDstQueue.subresourceRange.aspectMask =
        imgTransferInfo.aspectMask;
    barrierTransitionToDstQueue.subresourceRange.baseMipLevel = 0;
    barrierTransitionToDstQueue.subresourceRange.levelCount =
        VK_REMAINING_MIP_LEVELS;
    barrierTransitionToDstQueue.subresourceRange.baseArrayLayer = 0;
    barrierTransitionToDstQueue.subresourceRange.layerCount =
        imgTransferInfo.layerCount;

    // release queue ownership barrier
    vkCmdPipelineBarrier(cmdTransfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrierTransitionToDstQueue);

    VkSemaphore transitionToDstQueueSemaphore = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToDstQueueSemaphore));
    transfer.semaphores.push_back(transitionToDstQueueSemaphore);

    transferSubmitInfo.signalSemaphoreCount = 1;
    transferSubmitInfo.pSignalSemaphores = &transitionToDstQueueSemaphore;

    VK_CHECK(vkEndCommandBuffer(cmdTransfer));
    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues[_transferQueueFamilyIndex], 1,
                             &transferSubmitInfo, VK_NULL_HANDLE));
    }

    VkCommandBuffer cmdAcquire = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo const cmdAcquireAllocInfo =
        vkinit::commandBufferAllocateInfo(
            ctx.queueCommandPools.at(transferDstState.dstImgQueueFamilyIdx));

    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdAcquireAllocInfo,
                                      &cmdAcquire));
    transfer.commandBuffers.push_back(cmdAcquire);

    VkCommandBufferBeginInfo cmdAcquireBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdAcquire, &cmdAcquireBeginInfo));

    // acquire ownership of the dst queue
    barrierTransitionToDstQueue.srcAccessMask = VK_ACCESS_NONE;
    barrierTransitionToDstQueue.dstAccessMask = transferDstState.dstAccessMask;
    vkCmdPipelineBarrier(cmdAcquire, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         transferDstState.dstPipelineStage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrierTransitionToDstQueue);

    VK_CHECK(vkEndCommandBuffer(cmdAcquire));

    VkSubmitInfo acquireSubmitInfo = {};
    acquireSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    acquireSubmitInfo.pNext = nullptr;

    acquireSubmitInfo.commandBufferCount = 1;
    acquireSubmitInfo.pCommandBuffers = &cmdAcquire;
    acquireSubmitInfo.waitSemaphoreCount = 1;
    acquireSubmitInfo.pWaitSemaphores = &transitionToDstQueueSemaphore;
    VkPipelineStageFlags waitSemaphoreStageFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    acquireSubmitInfo.pWaitDstStageMask = &waitSemaphoreStageFlags;

    {
      std::scoped_lock l{
          _gpuQueueMutexes.at(transferDstState.dstImgQueueFamilyIdx)};
      VK_CHECK(
          vkQueueSubmit(_gpuQueues.at(transferDstState.dstImgQueueFamilyIdx), 1,
                        &acquireSubmitInfo, transfer.transferFence));
    }
  } else {
    VK_CHECK(vkEndCommandBuffer(cmdTransfer));

    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues[_transferQueueFamilyIndex], 1,
                             &transferSubmitInfo, transfer.transferFence));
    }
  }

  {
    std::scoped_lock l{_resourceTransfersMutex};
    _resourceTransfers.push_back(transfer);
  }
}

void VulkanRHI::transferDataToBuffer(
    AllocatedBuffer stagingBuffer, VkBuffer dstBuffer,
    BufferTransferInfo const& bufferTransferInfo,
    std::uint32_t currentBufferQeueueFamilyIdx,
    BufferTransferDstState transferDstState) {
  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  ResourceTransfer transfer = {};
  transfer.stagingBuffer = stagingBuffer;

  VkFenceCreateInfo fenceCreateInfo = {.sType =
                                           VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = 0};

  VK_CHECK(vkCreateFence(ctx.device, &fenceCreateInfo, nullptr,
                         &transfer.transferFence));

  VkSemaphore transitionToTransferQueueSemaphore = VK_NULL_HANDLE;

  VkCommandBuffer cmdTransfer = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo const cmdTransferAllocInfo =
      vkinit::commandBufferAllocateInfo(
          ctx.queueCommandPools.at(_transferQueueFamilyIndex));

  VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdTransferAllocInfo,
                                    &cmdTransfer));
  transfer.commandBuffers.push_back(cmdTransfer);

  VkCommandBufferBeginInfo const cmdTransferBegininfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmdTransfer, &cmdTransferBegininfo));

  VkBufferMemoryBarrier barrierTransitionToTransferQueue = {};
  barrierTransitionToTransferQueue.sType =
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrierTransitionToTransferQueue.pNext = nullptr;

  barrierTransitionToTransferQueue.srcAccessMask = VK_ACCESS_NONE;
  barrierTransitionToTransferQueue.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrierTransitionToTransferQueue.srcQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrierTransitionToTransferQueue.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrierTransitionToTransferQueue.buffer = dstBuffer;
  barrierTransitionToTransferQueue.offset = bufferTransferInfo.offset;
  barrierTransitionToTransferQueue.size = bufferTransferInfo.size;

  if (currentBufferQeueueFamilyIdx != VK_QUEUE_FAMILY_IGNORED &&
      currentBufferQeueueFamilyIdx != _transferQueueFamilyIndex) {
    // Transition queue ownership from current queue to transfer queue
    barrierTransitionToTransferQueue.srcQueueFamilyIndex =
        currentBufferQeueueFamilyIdx;
    barrierTransitionToTransferQueue.dstQueueFamilyIndex =
        _transferQueueFamilyIndex;

    VkCommandBuffer cmdRelease = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo const cmdReleaseAllocInfo =
        vkinit::commandBufferAllocateInfo(
            ctx.queueCommandPools.at(currentBufferQeueueFamilyIdx));

    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdReleaseAllocInfo,
                                      &cmdRelease));
    transfer.commandBuffers.push_back(cmdRelease);

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToTransferQueueSemaphore));
    transfer.semaphores.push_back(transitionToTransferQueueSemaphore);

    VkCommandBufferBeginInfo cmdBufferBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdRelease, &cmdBufferBeginInfo));

    vkCmdPipelineBarrier(cmdRelease, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &barrierTransitionToTransferQueue, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmdRelease));

    VkSubmitInfo releaseSubmitInfo = {};
    releaseSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    releaseSubmitInfo.pNext = nullptr;

    releaseSubmitInfo.commandBufferCount = 1;
    releaseSubmitInfo.pCommandBuffers = &cmdRelease;
    releaseSubmitInfo.signalSemaphoreCount = 1;
    releaseSubmitInfo.pSignalSemaphores = &transitionToTransferQueueSemaphore;

    {
      std::scoped_lock l{_gpuQueueMutexes.at(currentBufferQeueueFamilyIdx)};
      VK_CHECK(vkQueueSubmit(_gpuQueues.at(currentBufferQeueueFamilyIdx), 1,
                             &releaseSubmitInfo, VK_NULL_HANDLE));
    }
  }

  vkCmdPipelineBarrier(cmdTransfer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                       &barrierTransitionToTransferQueue, 0, nullptr);
  VkBufferCopy const bufferCopy{.srcOffset = 0,
                                .dstOffset = bufferTransferInfo.offset,
                                .size = bufferTransferInfo.size};

  vkCmdCopyBuffer(cmdTransfer, stagingBuffer.buffer, dstBuffer, 1, &bufferCopy);

  VkSubmitInfo transferSubmitInfo = {};
  transferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  transferSubmitInfo.pNext = nullptr;

  if (transitionToTransferQueueSemaphore != VK_NULL_HANDLE) {
    transferSubmitInfo.waitSemaphoreCount = 1;
    transferSubmitInfo.pWaitSemaphores = &transitionToTransferQueueSemaphore;
  }

  VkPipelineStageFlags waitDstStageFlag = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  transferSubmitInfo.pWaitDstStageMask = &waitDstStageFlag;
  transferSubmitInfo.commandBufferCount = 1;
  transferSubmitInfo.pCommandBuffers = &cmdTransfer;

  if (transferDstState.dstBufferQueueFamilyIdx != VK_QUEUE_FAMILY_IGNORED &&
      transferDstState.dstBufferQueueFamilyIdx != _transferQueueFamilyIndex) {
    // transition image ownership to dst queue family with barriers

    VkBufferMemoryBarrier barrierTransitionToDstQueue = {};
    barrierTransitionToDstQueue.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrierTransitionToDstQueue.pNext = nullptr;

    barrierTransitionToDstQueue.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierTransitionToDstQueue.dstAccessMask = transferDstState.dstAccessMask;
    barrierTransitionToDstQueue.srcQueueFamilyIndex = _transferQueueFamilyIndex;
    barrierTransitionToDstQueue.dstQueueFamilyIndex =
        transferDstState.dstBufferQueueFamilyIdx;
    barrierTransitionToDstQueue.buffer = dstBuffer;
    barrierTransitionToDstQueue.offset = bufferTransferInfo.offset;
    barrierTransitionToDstQueue.size = bufferTransferInfo.size;

    // release queue ownership barrier
    vkCmdPipelineBarrier(cmdTransfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 1,
                         &barrierTransitionToDstQueue, 0, nullptr);

    VkSemaphore transitionToDstQueueSemaphore = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToDstQueueSemaphore));
    transfer.semaphores.push_back(transitionToDstQueueSemaphore);

    transferSubmitInfo.signalSemaphoreCount = 1;
    transferSubmitInfo.pSignalSemaphores = &transitionToDstQueueSemaphore;

    VK_CHECK(vkEndCommandBuffer(cmdTransfer));
    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues.at(_transferQueueFamilyIndex), 1,
                             &transferSubmitInfo, VK_NULL_HANDLE));
    }

    VkCommandBuffer cmdAcquire = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo const cmdAcquireAllocInfo =
        vkinit::commandBufferAllocateInfo(
            ctx.queueCommandPools.at(transferDstState.dstBufferQueueFamilyIdx));

    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdAcquireAllocInfo,
                                      &cmdAcquire));
    transfer.commandBuffers.push_back(cmdAcquire);

    VkCommandBufferBeginInfo cmdAcquireBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdAcquire, &cmdAcquireBeginInfo));

    // acquire ownership of the dst queue
    vkCmdPipelineBarrier(cmdAcquire, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         transferDstState.dstPipelineStage, 0, 0, nullptr, 1,
                         &barrierTransitionToDstQueue, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmdAcquire));

    VkSubmitInfo acquireSubmitInfo = {};
    acquireSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    acquireSubmitInfo.pNext = nullptr;

    acquireSubmitInfo.commandBufferCount = 1;
    acquireSubmitInfo.pCommandBuffers = &cmdAcquire;
    acquireSubmitInfo.waitSemaphoreCount = 1;
    acquireSubmitInfo.pWaitSemaphores = &transitionToDstQueueSemaphore;
    VkPipelineStageFlags waitSemaphoreStageFlags =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    acquireSubmitInfo.pWaitDstStageMask = &waitSemaphoreStageFlags;

    {
      std::scoped_lock l{
          _gpuQueueMutexes.at(transferDstState.dstBufferQueueFamilyIdx)};
      VK_CHECK(
          vkQueueSubmit(_gpuQueues.at(transferDstState.dstBufferQueueFamilyIdx),
                        1, &acquireSubmitInfo, transfer.transferFence));
    }
  } else {
    VK_CHECK(vkEndCommandBuffer(cmdTransfer));

    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues[_transferQueueFamilyIndex], 1,
                             &transferSubmitInfo, transfer.transferFence));
    }
  }

  {
    std::scoped_lock l{_resourceTransfersMutex};
    _resourceTransfers.push_back(transfer);
  }
}
FrameData& VulkanRHI::getCurrentFrameData() {
  std::size_t const currentFrameDataInd = _frameNumber % frameOverlap;
  return _frameDataArray[currentFrameDataInd];
}

FrameData& VulkanRHI::getPreviousFrameData() {
  std::size_t const previousFrameDataInd =
      (_frameNumber + frameOverlap - 1) % frameOverlap;

  return _frameDataArray[previousFrameDataInd];
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

std::vector<ShadowPassParams>
VulkanRHI::getSubmittedShadowPassParams(glm::vec3 mainCameraPos) const {
  std::vector<ShadowPassParams> result;

  for (rhi::DirectionalLight const& dirLight : _submittedDirectionalLights) {
    if (dirLight.assignedShadowMapInd < 0) {
      continue;
    }

    ShadowPassParams& param = result.emplace_back();
    param.shadowMapIndex = dirLight.assignedShadowMapInd;
    param.gpuCameraData = getDirectionalLightCameraData(
        dirLight.directionalLight.direction, mainCameraPos);
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

GPULightData VulkanRHI::getGPULightData(glm::vec3 mainCameraPos) const {
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
        _submittedDirectionalLights[i].directionalLight.direction,
        mainCameraPos);

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

VkExtent2D VulkanRHI::getSsaoExtent() const {
  return {_vkbSwapchain.extent.width / _ssaoResolutionDivider,
          _vkbSwapchain.extent.height / _ssaoResolutionDivider};
}

VkViewport VulkanRHI::getSsaoViewport() const {
  VkExtent2D const ssaoExtent = getSsaoExtent();
  return {0.0f,
          0.0f,
          static_cast<float>(ssaoExtent.width),
          static_cast<float>(ssaoExtent.height),
          0,
          1};
}

VkRect2D VulkanRHI::getSsaoScissor() const {
  VkExtent2D const ssaoExtent = getSsaoExtent();
  return {{0, 0}, ssaoExtent};
}

void VulkanRHI::applyPendingExtentUpdate() {
  if (_pendingExtentUpdate) {
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
    updateGlobalSettingsBuffer(false);
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

void VulkanRHI::updateGlobalSettingsBuffer(bool init) {
  GPUGlobalSettings globalSettings = {};
  globalSettings.swapchainWidth = _vkbSwapchain.extent.width;
  globalSettings.swapchainHeight = _vkbSwapchain.extent.height;

  uploadBufferData(0, globalSettings, _globalSettingsBuffer,
                   init ? VK_QUEUE_FAMILY_IGNORED : _graphicsQueueFamilyIndex);
}
