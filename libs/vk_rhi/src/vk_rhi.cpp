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

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>

using namespace obsidian;
using namespace obsidian::vk_rhi;

static thread_local std::unordered_map<std::uint32_t, ImmediateSubmitContext>
    immediateSubmitContext;
static thread_local bool immediateContextsInitialized = false;
static thread_local ResourceTransferContext resourceTransferCtx;

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
    std::scoped_lock l{_pendingResourcesToDestroyMutex};
    _pendingResourcesToDestroy.texturesToDestroy.push_back(
        {resourceIdRHI, _frameNumber.load()});
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

        setDbgResourceName(_vkDevice, (std::uint64_t)stagingBuffer.buffer,
                           VK_OBJECT_TYPE_BUFFER, "Mesh upload staging buffer");

        void* mappedMemory;
        vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

        {
          ZoneScopedN("Unpack Mesh");
          info.unpackFunc(reinterpret_cast<char*>(mappedMemory));
        }

        vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

        std::vector<BufferTransferInfo> bufferTransferInfos = {
            {.srcOffset = 0,
             .dstOffset = 0,
             .size = info.vertexBufferSize,
             .dstBuffer = mesh.vertexBuffer.buffer},
            {.srcOffset = info.vertexBufferSize,
             .dstOffset = 0,
             .size = totalIndexBufferSize,
             .dstBuffer = mesh.indexBuffer.buffer}};

        BufferTransferOptions bufferTransferOptions = {
            .dstBufferQueueFamilyIdx = _graphicsQueueFamilyIndex,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstPipelineStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};

        transferDataToBuffer(stagingBuffer, bufferTransferInfos,
                             VK_QUEUE_FAMILY_IGNORED, bufferTransferOptions);

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
    std::scoped_lock l{_pendingResourcesToDestroyMutex};
    _pendingResourcesToDestroy.meshesToDestroy.push_back(
        {resourceIdRHI, _frameNumber.load()});
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
    std::scoped_lock l{_pendingResourcesToDestroyMutex};
    _pendingResourcesToDestroy.shadersToDestroy.push_back(
        {resourceIdRHI, _frameNumber.load()});
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

  return rhi::ResourceTransferRHI{
      _taskExecutor
          .enqueue(task::TaskType::rhiTransfer,
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
                             VK_SHADER_STAGE_VERTEX_BIT,
                             vertexShaderModule.vkShaderModule));

                     Shader& fragmentShaderModule =
                         _shaderModules.at(uploadMaterial.fragmentShaderId);
                     ++fragmentShaderModule.resource.refCount;
                     newMaterial.fragmentShaderResourceDependencyId =
                         fragmentShaderModule.resource.id;

                     pipelineBuilder._vkShaderStageCreateInfos.push_back(
                         vkinit::pipelineShaderStageCreateInfo(
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                             fragmentShaderModule.vkShaderModule));

                     newMaterial.vkPipelineLayout =
                         pipelineBuilder._vkPipelineLayout;

                     pipelineBuilder._vkDepthStencilStateCreateInfo =
                         vkinit::
                             depthStencilStateCreateInfo(
                                 true, _sampleCount != VK_SAMPLE_COUNT_1_BIT /*we don't reuse depth in case of multisampling*/);
                     newMaterial.vkPipelineMainRenderPass =
                         pipelineBuilder.buildPipeline(_vkDevice,
                                                       _mainRenderPass);

                     setDbgResourceName(
                         _vkDevice,
                         (std::uint64_t)newMaterial.vkPipelineMainRenderPass,
                         VK_OBJECT_TYPE_PIPELINE, uploadMaterial.debugName,
                         "Reuse depth pipeline");

                     pipelineBuilder._vkDepthStencilStateCreateInfo =
                         vkinit::depthStencilStateCreateInfo(true, true);
                     pipelineBuilder._vkRasterizationCreateInfo.frontFace =
                         VK_FRONT_FACE_CLOCKWISE;
                     newMaterial.vkPipelineEnvironmentRendering =
                         pipelineBuilder.buildPipeline(_vkDevice,
                                                       _envMapRenderPass);
                     pipelineBuilder._vkRasterizationCreateInfo.frontFace =
                         VK_FRONT_FACE_COUNTER_CLOCKWISE;

                     setDbgResourceName(
                         _vkDevice,
                         (std::uint64_t)
                             newMaterial.vkPipelineEnvironmentRendering,
                         VK_OBJECT_TYPE_PIPELINE, uploadMaterial.debugName,
                         "Environment rendering pipeline");

                     newMaterial.transparent = uploadMaterial.transparent;

                     DescriptorBuilder descriptorBuilder =
                         DescriptorBuilder::begin(_vkDevice,
                                                  _descriptorAllocator,
                                                  _descriptorLayoutCache);

                     VkDescriptorBufferInfo materialDataBufferInfo;

                     if (uploadMaterial.materialType ==
                         core::MaterialType::lit) {
                       rhi::UploadLitMaterialRHI const& uploadLitMaterial =
                           std::get<rhi::UploadLitMaterialRHI>(
                               uploadMaterial.uploadMaterialSubtype);

                       newMaterial.reflection = uploadLitMaterial.reflection;

                       GPULitMaterialData materialData;
                       materialData.hasDiffuseTex =
                           uploadLitMaterial.diffuseTextureId !=
                           rhi::rhiIdUninitialized;
                       materialData.hasNormalMap =
                           uploadLitMaterial.normalTextureId !=
                           rhi::rhiIdUninitialized;
                       materialData.reflection = uploadLitMaterial.reflection;
                       materialData.ambientColor =
                           uploadLitMaterial.ambientColor;
                       materialData.diffuseColor =
                           uploadLitMaterial.diffuseColor;
                       materialData.specularColor =
                           uploadLitMaterial.specularColor;
                       materialData.shininess = uploadLitMaterial.shininess;

                       createAndBindMaterialDataBuffer(materialData,
                                                       descriptorBuilder,
                                                       materialDataBufferInfo);

                       bool const hasDiffuseTex =
                           uploadLitMaterial.diffuseTextureId !=
                           rhi::rhiIdUninitialized;

                       if (hasDiffuseTex) {
                         Texture& diffuseTexture =
                             _textures[uploadLitMaterial.diffuseTextureId];

                         ++diffuseTexture.resource.refCount;
                         newMaterial.textureResourceDependencyIds.push_back(
                             diffuseTexture.resource.id);

                         VkDescriptorImageInfo diffuseTexImageInfo;
                         diffuseTexImageInfo.imageLayout =
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                         diffuseTexImageInfo.imageView =
                             diffuseTexture.imageView;
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
                           uploadLitMaterial.normalTextureId !=
                           rhi::rhiIdUninitialized;

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

                         normalMapTexImageInfo.imageView =
                             normalMapTexture.imageView;

                         descriptorBuilder.bindImage(
                             2, normalMapTexImageInfo,
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
                       } else {
                         descriptorBuilder.declareUnusedImage(
                             2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);
                       }
                     } else if (uploadMaterial.materialType ==
                                core::MaterialType::unlit) {
                       rhi::UploadUnlitMaterialRHI const& uploadUnlitMaterial =
                           std::get<rhi::UploadUnlitMaterialRHI>(
                               uploadMaterial.uploadMaterialSubtype);
                       GPUUnlitMaterialData materialData;
                       materialData.hasColorTex =
                           uploadUnlitMaterial.colorTextureId !=
                           rhi::rhiIdUninitialized;
                       materialData.color = uploadUnlitMaterial.color;

                       createAndBindMaterialDataBuffer(materialData,
                                                       descriptorBuilder,
                                                       materialDataBufferInfo);

                       bool const hasColorTex =
                           uploadUnlitMaterial.colorTextureId !=
                           rhi::rhiIdUninitialized;
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
                             1, colorTexImageInfo,
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true);
                       } else {
                         descriptorBuilder.declareUnusedImage(
                             1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);
                       }
                     } else if (uploadMaterial.materialType ==
                                core::MaterialType::pbr) {
                       rhi::UploadPBRMaterialRHI const& uploadPbrMaterial =
                           std::get<rhi::UploadPBRMaterialRHI>(
                               uploadMaterial.uploadMaterialSubtype);

                       GPUPbrMaterialData pbrMaterialData;
                       pbrMaterialData.metalnessAndRoughnessSeparate =
                           uploadPbrMaterial.roughnessTextureId !=
                           rhi::rhiIdUninitialized;

                       createAndBindMaterialDataBuffer(pbrMaterialData,
                                                       descriptorBuilder,
                                                       materialDataBufferInfo);

                       Texture& albedoTexture =
                           _textures[uploadPbrMaterial.albedoTextureId];

                       ++albedoTexture.resource.refCount;
                       newMaterial.textureResourceDependencyIds.push_back(
                           albedoTexture.resource.id);

                       VkDescriptorImageInfo albedoTexImageInfo;
                       albedoTexImageInfo.imageLayout =
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                       albedoTexImageInfo.imageView = albedoTexture.imageView;
                       albedoTexImageInfo.sampler = _vkLinearRepeatSampler;

                       descriptorBuilder.bindImage(
                           1, albedoTexImageInfo,
                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

                       Texture& normalTexture =
                           _textures[uploadPbrMaterial.normalTextureId];

                       ++normalTexture.resource.refCount;
                       newMaterial.textureResourceDependencyIds.push_back(
                           normalTexture.resource.id);

                       VkDescriptorImageInfo normalTexImageInfo;
                       normalTexImageInfo.imageLayout =
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                       normalTexImageInfo.imageView = normalTexture.imageView;
                       normalTexImageInfo.sampler = _vkLinearRepeatSampler;

                       descriptorBuilder.bindImage(
                           2, normalTexImageInfo,
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
                       metalnessTexImageInfo.imageView =
                           metalnessTexture.imageView;
                       metalnessTexImageInfo.sampler = _vkLinearRepeatSampler;

                       descriptorBuilder.bindImage(
                           3, metalnessTexImageInfo,
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
                         roughnessTexImageInfo.imageView =
                             metalnessTexture.imageView;
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

                     if (!descriptorBuilder.build(
                             newMaterial.vkDescriptorSet)) {
                       OBS_LOG_ERR("Failed to build descriptor set when "
                                   "uploading material");
                       newMaterial.resource.state = rhi::ResourceState::invalid;
                     } else {
                       rhi::ResourceState expected =
                           rhi::ResourceState::uploading;

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
    std::size_t const frameNumber = _frameNumber.load();

    std::scoped_lock l{_pendingResourcesToDestroyMutex};

    _pendingResourcesToDestroy.materialsToDestroy.push_back(
        {resourceIdRHI, frameNumber});

    Shader& vertexShaderDependency =
        _shaderModules.at(material.vertexShaderResourceDependencyId);

    if (!--vertexShaderDependency.resource.refCount) {
      _pendingResourcesToDestroy.shadersToDestroy.push_back(
          {material.vertexShaderResourceDependencyId, frameNumber});
    }

    Shader& fragmentShaderDependency =
        _shaderModules.at(material.fragmentShaderResourceDependencyId);

    if (!--fragmentShaderDependency.resource.refCount) {
      _pendingResourcesToDestroy.shadersToDestroy.push_back(
          {material.fragmentShaderResourceDependencyId, frameNumber});
    }

    for (rhi::ResourceIdRHI textureId : material.textureResourceDependencyIds) {
      Texture& textureDependency = _textures.at(textureId);

      if (!--textureDependency.resource.refCount) {
        _pendingResourcesToDestroy.texturesToDestroy.push_back(
            {textureId, frameNumber});
      }
    }
  }
}

void VulkanRHI::destroyUnusedResources(bool forceDestroy) {
  // clear materials
  PendingResourcesToDestroy toDestroy = {};
  PendingResourcesToDestroy persisted = {};

  {
    std::scoped_lock l{_pendingResourcesToDestroyMutex};
    std::swap(toDestroy, _pendingResourcesToDestroy);
  }

  std::uint64_t lastRenderedFrame;
  VK_CHECK(vkGetSemaphoreCounterValue(_vkDevice, _frameNumberSemaphore,
                                      &lastRenderedFrame));

  for (PendingResourcesToDestroy::ResourceEntry const& matEntry :
       toDestroy.materialsToDestroy) {
    if (forceDestroy || matEntry.lastUsedFrame <= lastRenderedFrame) {
      VkMaterial& mat = _materials.at(matEntry.id);

      if (mat.vkPipelineMainRenderPass) {
        vkDestroyPipeline(_vkDevice, mat.vkPipelineMainRenderPass, nullptr);
      }
      if (mat.vkPipelineEnvironmentRendering) {
        vkDestroyPipeline(_vkDevice, mat.vkPipelineEnvironmentRendering,
                          nullptr);
      }

      _materials.erase(matEntry.id);
    } else {
      persisted.materialsToDestroy.push_back(matEntry);
    }
  }

  // clear textures
  for (PendingResourcesToDestroy::ResourceEntry const& texEntry :
       toDestroy.texturesToDestroy) {
    if (forceDestroy || texEntry.lastUsedFrame <= lastRenderedFrame) {
      Texture& tex = _textures.at(texEntry.id);

      vkDestroyImageView(_vkDevice, tex.imageView, nullptr);
      vmaDestroyImage(_vmaAllocator, tex.image.vkImage, tex.image.allocation);

      _textures.erase(texEntry.id);
    } else {
      persisted.texturesToDestroy.push_back(texEntry);
    }
  }

  // clear shaders
  for (PendingResourcesToDestroy::ResourceEntry const& shaderEntry :
       toDestroy.shadersToDestroy) {
    if (forceDestroy || shaderEntry.lastUsedFrame <= lastRenderedFrame) {
      Shader& shader = _shaderModules.at(shaderEntry.id);
      vkDestroyShaderModule(_vkDevice, shader.vkShaderModule, nullptr);

      _shaderModules.erase(shaderEntry.id);
    } else {
      persisted.shadersToDestroy.push_back(shaderEntry);
    }
  }

  // clear meshes
  for (PendingResourcesToDestroy::ResourceEntry const& meshEntry :
       toDestroy.meshesToDestroy) {
    if (forceDestroy || meshEntry.lastUsedFrame <= lastRenderedFrame) {
      Mesh& mesh = _meshes.at(meshEntry.id);

      vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                       mesh.vertexBuffer.allocation);

      vmaDestroyBuffer(_vmaAllocator, mesh.indexBuffer.buffer,
                       mesh.indexBuffer.allocation);

      _meshes.erase(meshEntry.id);

    } else {
      persisted.meshesToDestroy.push_back(meshEntry);
    }
  }

  // clear environment maps
  for (PendingResourcesToDestroy::ResourceEntry const& envMapEntry :
       toDestroy.environmentMapsToDestroy) {
    assert(_environmentMaps.contains(envMapEntry.id));

    if (forceDestroy || envMapEntry.lastUsedFrame <= lastRenderedFrame) {
      EnvironmentMap& envMap = _environmentMaps.at(envMapEntry.id);

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

      _environmentMaps.erase(envMapEntry.id);
    } else {
      persisted.environmentMapsToDestroy.push_back(envMapEntry);
    }
  }

  bool const persistedEmpty = persisted.texturesToDestroy.empty() &&
                              persisted.meshesToDestroy.empty() &&
                              persisted.environmentMapsToDestroy.empty() &&
                              persisted.materialsToDestroy.empty() &&
                              persisted.shadersToDestroy.empty();

  if (!persistedEmpty) {
    std::scoped_lock l{_pendingResourcesToDestroyMutex};

    _pendingResourcesToDestroy.texturesToDestroy.insert(
        _pendingResourcesToDestroy.texturesToDestroy.end(),
        persisted.texturesToDestroy.cbegin(),
        persisted.texturesToDestroy.cend());
    _pendingResourcesToDestroy.meshesToDestroy.insert(
        _pendingResourcesToDestroy.meshesToDestroy.end(),
        persisted.meshesToDestroy.cbegin(), persisted.meshesToDestroy.cend());
    _pendingResourcesToDestroy.environmentMapsToDestroy.insert(
        _pendingResourcesToDestroy.environmentMapsToDestroy.end(),
        persisted.environmentMapsToDestroy.cbegin(),
        persisted.environmentMapsToDestroy.cend());
    _pendingResourcesToDestroy.materialsToDestroy.insert(
        _pendingResourcesToDestroy.materialsToDestroy.end(),
        persisted.materialsToDestroy.cbegin(),
        persisted.materialsToDestroy.cend());
    _pendingResourcesToDestroy.shadersToDestroy.insert(
        _pendingResourcesToDestroy.shadersToDestroy.end(),
        persisted.shadersToDestroy.cbegin(), persisted.shadersToDestroy.cend());
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
  std::scoped_lock l{_pendingExtentUpdateMutex};
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
  ctx.cleanupFunction = [this]() {
    cleanupFinishedTransfersForCurrentThread(true);
  };

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

void VulkanRHI::cleanupFinishedTransfersForCurrentThread(bool waitToFinish) {
  static thread_local std::vector<TransferResources> transferResources;

  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  transferResources.swap(ctx.transferResources);

  for (auto const& t : transferResources) {
    VkResult result;
    if (waitToFinish) {
      result = vkWaitForFences(_vkDevice, 1, &t.transferFence, VK_TRUE,
                               1000'000'000);
      VK_CHECK(result);
    } else {
      result = vkGetFenceStatus(_vkDevice, t.transferFence);
    }
    if (result == VK_SUCCESS) {
      vmaDestroyBuffer(_vmaAllocator, t.stagingBuffer.buffer,
                       t.stagingBuffer.allocation);

      vkDestroyFence(_vkDevice, t.transferFence, nullptr);

      for (VkSemaphore s : t.semaphores) {
        vkDestroySemaphore(_vkDevice, s, nullptr);
      }

      for (TransferResources::CmdBufferPoolPair b : t.commandBuffers) {
        vkFreeCommandBuffers(_vkDevice, b.pool, 1, &b.buffer);
      }
    } else if (result == VK_NOT_READY) {
      ctx.transferResources.push_back(t);
    } else {
      VK_CHECK(result);
    }
  }

  transferResources.clear();
}

ResourceTransferContext&
VulkanRHI::getResourceTransferContextForCurrentThread() {
  if (!resourceTransferCtx.initialized) {
    initResourceTransferContext(resourceTransferCtx);
  }

  return resourceTransferCtx;
}

void VulkanRHI::transferDataToImage(AllocatedBuffer stagingBuffer,
                                    VkImage dstImg,
                                    ImageTransferInfo const& imgTransferInfo,
                                    std::uint32_t currentImgQueueFamilyIdx,
                                    ImageTransferDstState transferDstState) {
  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  TransferResources resources = {};
  resources.stagingBuffer = stagingBuffer;

  VkFenceCreateInfo fenceCreateInfo = {.sType =
                                           VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = 0};

  VK_CHECK(vkCreateFence(ctx.device, &fenceCreateInfo, nullptr,
                         &resources.transferFence));

  VkSemaphore transitionToTransferQueueSemaphore = VK_NULL_HANDLE;

  VkCommandBuffer cmdTransfer = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo const cmdTransferAllocInfo =
      vkinit::commandBufferAllocateInfo(
          ctx.queueCommandPools.at(_transferQueueFamilyIndex));

  VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdTransferAllocInfo,
                                    &cmdTransfer));
  resources.commandBuffers.push_back(
      {cmdTransfer, cmdTransferAllocInfo.commandPool});

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
    resources.commandBuffers.push_back(
        {cmdRelease, cmdReleaseAllocInfo.commandPool});

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToTransferQueueSemaphore));
    resources.semaphores.push_back(transitionToTransferQueueSemaphore);

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
    resources.semaphores.push_back(transitionToDstQueueSemaphore);

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
    resources.commandBuffers.push_back(
        {cmdAcquire, cmdAcquireAllocInfo.commandPool});

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
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    acquireSubmitInfo.pWaitDstStageMask = &waitSemaphoreStageFlags;

    {
      std::scoped_lock l{
          _gpuQueueMutexes.at(transferDstState.dstImgQueueFamilyIdx)};
      VK_CHECK(
          vkQueueSubmit(_gpuQueues.at(transferDstState.dstImgQueueFamilyIdx), 1,
                        &acquireSubmitInfo, resources.transferFence));
    }
  } else {
    VK_CHECK(vkEndCommandBuffer(cmdTransfer));

    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues[_transferQueueFamilyIndex], 1,
                             &transferSubmitInfo, resources.transferFence));
    }
  }

  ctx.transferResources.push_back(resources);
}

void VulkanRHI::transferDataToBuffer(
    AllocatedBuffer stagingBuffer,
    std::vector<BufferTransferInfo> const& bufferTransferInfos,
    std::uint32_t currentBufferQeueueFamilyIdx,
    BufferTransferOptions bufferTransferOptions) {
  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  TransferResources transferResources = {};
  transferResources.stagingBuffer = stagingBuffer;

  VkFenceCreateInfo fenceCreateInfo = {.sType =
                                           VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = 0};

  VK_CHECK(vkCreateFence(ctx.device, &fenceCreateInfo, nullptr,
                         &transferResources.transferFence));

  VkSemaphore transitionToTransferQueueSemaphore = VK_NULL_HANDLE;

  VkCommandBuffer cmdTransfer = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo const cmdTransferAllocInfo =
      vkinit::commandBufferAllocateInfo(
          ctx.queueCommandPools.at(_transferQueueFamilyIndex));

  VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdTransferAllocInfo,
                                    &cmdTransfer));
  transferResources.commandBuffers.push_back(
      {cmdTransfer, cmdTransferAllocInfo.commandPool});

  VkCommandBufferBeginInfo const cmdTransferBegininfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmdTransfer, &cmdTransferBegininfo));

  VkBufferMemoryBarrier barrierTransitionToTransferQueue = {};
  barrierTransitionToTransferQueue.sType =
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrierTransitionToTransferQueue.pNext = nullptr;

  barrierTransitionToTransferQueue.srcAccessMask =
      bufferTransferOptions.srcAccessMask;
  barrierTransitionToTransferQueue.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrierTransitionToTransferQueue.srcQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrierTransitionToTransferQueue.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;

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
    transferResources.commandBuffers.push_back(
        {cmdRelease, cmdReleaseAllocInfo.commandPool});

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToTransferQueueSemaphore));
    transferResources.semaphores.push_back(transitionToTransferQueueSemaphore);

    VkCommandBufferBeginInfo cmdBufferBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdRelease, &cmdBufferBeginInfo));

    for (BufferTransferInfo const& bufferTransferInfo : bufferTransferInfos) {
      barrierTransitionToTransferQueue.buffer = bufferTransferInfo.dstBuffer;
      barrierTransitionToTransferQueue.size = bufferTransferInfo.size;
      barrierTransitionToTransferQueue.offset = bufferTransferInfo.dstOffset;

      vkCmdPipelineBarrier(cmdRelease, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                           &barrierTransitionToTransferQueue, 0, nullptr);
    }

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

  for (BufferTransferInfo const& bufferTransferInfo : bufferTransferInfos) {
    barrierTransitionToTransferQueue.buffer = bufferTransferInfo.dstBuffer;
    barrierTransitionToTransferQueue.offset = bufferTransferInfo.dstOffset;
    barrierTransitionToTransferQueue.size = bufferTransferInfo.size;
    vkCmdPipelineBarrier(cmdTransfer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &barrierTransitionToTransferQueue, 0, nullptr);

    VkBufferCopy const bufferCopy{.srcOffset = bufferTransferInfo.srcOffset,
                                  .dstOffset = bufferTransferInfo.dstOffset,
                                  .size = bufferTransferInfo.size};

    vkCmdCopyBuffer(cmdTransfer, stagingBuffer.buffer,
                    bufferTransferInfo.dstBuffer, 1, &bufferCopy);
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

  if (bufferTransferOptions.dstBufferQueueFamilyIdx !=
          VK_QUEUE_FAMILY_IGNORED &&
      bufferTransferOptions.dstBufferQueueFamilyIdx !=
          _transferQueueFamilyIndex) {
    // transition image ownership to dst queue family with barriers

    VkBufferMemoryBarrier barrierTransitionToDstQueue = {};
    barrierTransitionToDstQueue.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrierTransitionToDstQueue.pNext = nullptr;

    barrierTransitionToDstQueue.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierTransitionToDstQueue.dstAccessMask =
        bufferTransferOptions.dstAccessMask;
    barrierTransitionToDstQueue.srcQueueFamilyIndex = _transferQueueFamilyIndex;
    barrierTransitionToDstQueue.dstQueueFamilyIndex =
        bufferTransferOptions.dstBufferQueueFamilyIdx;

    for (BufferTransferInfo const& bufferTransferInfo : bufferTransferInfos) {
      barrierTransitionToDstQueue.offset = bufferTransferInfo.dstOffset;
      barrierTransitionToDstQueue.size = bufferTransferInfo.size;
      barrierTransitionToDstQueue.buffer = bufferTransferInfo.dstBuffer;

      // release queue ownership barrier
      vkCmdPipelineBarrier(
          cmdTransfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
          /*dstStageFlag should be ignored by the API in this case, but
             validation layers complain if this isn't set to all commands*/
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1,
          &barrierTransitionToDstQueue, 0, nullptr);
    }

    VkSemaphore transitionToDstQueueSemaphore = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo const semaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr,
                               &transitionToDstQueueSemaphore));
    transferResources.semaphores.push_back(transitionToDstQueueSemaphore);

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
        vkinit::commandBufferAllocateInfo(ctx.queueCommandPools.at(
            bufferTransferOptions.dstBufferQueueFamilyIdx));

    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmdAcquireAllocInfo,
                                      &cmdAcquire));
    transferResources.commandBuffers.push_back(
        {cmdAcquire, cmdAcquireAllocInfo.commandPool});

    VkCommandBufferBeginInfo cmdAcquireBeginInfo =
        vkinit::commandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdAcquire, &cmdAcquireBeginInfo));

    // acquire ownership of the dst queue
    vkCmdPipelineBarrier(cmdAcquire, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         bufferTransferOptions.dstPipelineStage, 0, 0, nullptr,
                         1, &barrierTransitionToDstQueue, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmdAcquire));

    VkSubmitInfo acquireSubmitInfo = {};
    acquireSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    acquireSubmitInfo.pNext = nullptr;

    acquireSubmitInfo.commandBufferCount = 1;
    acquireSubmitInfo.pCommandBuffers = &cmdAcquire;
    acquireSubmitInfo.waitSemaphoreCount = 1;
    acquireSubmitInfo.pWaitSemaphores = &transitionToDstQueueSemaphore;
    VkPipelineStageFlags waitSemaphoreStageFlags =
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    acquireSubmitInfo.pWaitDstStageMask = &waitSemaphoreStageFlags;

    {
      std::scoped_lock l{
          _gpuQueueMutexes.at(bufferTransferOptions.dstBufferQueueFamilyIdx)};
      VK_CHECK(vkQueueSubmit(
          _gpuQueues.at(bufferTransferOptions.dstBufferQueueFamilyIdx), 1,
          &acquireSubmitInfo, transferResources.transferFence));
    }
  } else {
    VK_CHECK(vkEndCommandBuffer(cmdTransfer));

    {
      std::scoped_lock l{_gpuQueueMutexes.at(_transferQueueFamilyIndex)};
      VK_CHECK(vkQueueSubmit(_gpuQueues[_transferQueueFamilyIndex], 1,
                             &transferSubmitInfo,
                             transferResources.transferFence));
    }
  }

  ctx.transferResources.push_back(transferResources);
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
  std::scoped_lock l{_pendingExtentUpdateMutex};
  if (_pendingExtentUpdate) {
    waitDeviceIdle();

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

    clearFrameData();
    _pendingExtentUpdate = std::nullopt;
  }
}

ImmediateSubmitContext&
VulkanRHI::getImmediateCtxForCurrentThread(std::uint32_t queueIdx) {
  if (!immediateContextsInitialized) {
    for (auto const& q : _gpuQueues) {
      initImmediateSubmitContext(immediateSubmitContext[q.first], q.first);
    }

    immediateContextsInitialized = true;
  }

  return immediateSubmitContext[queueIdx];
}

// used on main thread to manually destroy the context
void VulkanRHI::destroyImmediateCtxForCurrentThread() {
  immediateSubmitContext = {};
  immediateContextsInitialized = false;
}

void VulkanRHI::cleanupResourceTransferCtxForCurrentThread() {
  ResourceTransferContext& ctx = getResourceTransferContextForCurrentThread();
  ctx.cleanup();
}

void VulkanRHI::updateGlobalSettingsBuffer(bool init) {
  GPUGlobalSettings globalSettings = {};
  globalSettings.swapchainWidth = _vkbSwapchain.extent.width;
  globalSettings.swapchainHeight = _vkbSwapchain.extent.height;

  uploadBufferData(0, globalSettings, _globalSettingsBuffer,
                   init ? VK_QUEUE_FAMILY_IGNORED : _graphicsQueueFamilyIndex);
}
