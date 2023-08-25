#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

using namespace obsidian;
using namespace obsidian::vk_rhi;

rhi::ResourceIdRHI
VulkanRHI::uploadTexture(rhi::UploadTextureRHI const& uploadTextureInfoRHI) {

  std::size_t const size =
      uploadTextureInfoRHI.width * uploadTextureInfoRHI.height *
      core::getFormatPixelSize(uploadTextureInfoRHI.format);

  AllocatedBuffer stagingBuffer = createBuffer(
      size, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,

      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  uploadTextureInfoRHI.unpackFunc(reinterpret_cast<char*>(mappedMemory));

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  AllocatedImage newImage;
  VkImageUsageFlags const imageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkExtent3D extent = {};
  extent.width = uploadTextureInfoRHI.width;
  extent.height = uploadTextureInfoRHI.height;
  extent.depth = 1;
  VkFormat const format = VK_FORMAT_R8G8B8A8_SRGB;

  VkImageCreateInfo vkImgCreateInfo =
      vkinit::imageCreateInfo(imageUsageFlags, extent, format);

  VmaAllocationCreateInfo imgAllocationCreateInfo = {};
  imgAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  vmaCreateImage(_vmaAllocator, &vkImgCreateInfo, &imgAllocationCreateInfo,
                 &newImage.vkImage, &newImage.allocation, nullptr);

  immediateSubmit([this, &extent, &newImage,
                   &stagingBuffer](VkCommandBuffer cmd) {
    VkImageMemoryBarrier vkImgBarrierToTransfer = {};
    vkImgBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImgBarrierToTransfer.pNext = nullptr;

    vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
    vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImgBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImgBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImgBarrierToTransfer.image = newImage.vkImage;

    VkImageSubresourceRange& vkImgSubresourceRangeToTransfer =
        vkImgBarrierToTransfer.subresourceRange;
    vkImgSubresourceRangeToTransfer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToTransfer.baseMipLevel = 0;
    vkImgSubresourceRangeToTransfer.levelCount = 1;
    vkImgSubresourceRangeToTransfer.baseArrayLayer = 0;
    vkImgSubresourceRangeToTransfer.layerCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &vkImgBarrierToTransfer);

    VkBufferImageCopy vkBufferImgCopy = {};
    vkBufferImgCopy.imageExtent = extent;
    vkBufferImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBufferImgCopy.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.vkImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &vkBufferImgCopy);

    VkImageMemoryBarrier vkImageBarrierToRead = {};
    vkImageBarrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageBarrierToRead.pNext = nullptr;

    vkImageBarrierToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImageBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkImageBarrierToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageBarrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkImageBarrierToRead.image = newImage.vkImage;
    vkImageBarrierToRead.srcQueueFamilyIndex = 0;
    vkImageBarrierToRead.dstQueueFamilyIndex = _graphicsQueueFamilyIndex;

    VkImageSubresourceRange& vkImgSubresourceRangeToRead =
        vkImageBarrierToRead.subresourceRange;
    vkImgSubresourceRangeToRead.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToRead.layerCount = 1;
    vkImgSubresourceRangeToRead.levelCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &vkImageBarrierToRead);
  });

  _deletionQueue.pushFunction([this, newImage]() {
    vmaDestroyImage(_vmaAllocator, newImage.vkImage, newImage.allocation);
  });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Texture& lostEmpireTexture = _texturesNew[newResourceId];

  VkImageViewCreateInfo lostEmpImgViewCreateInfo = vkinit::imageViewCreateInfo(
      lostEmpireTexture.image.vkImage, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT);

  vkCreateImageView(_vkDevice, &lostEmpImgViewCreateInfo, nullptr,
                    &lostEmpireTexture.imageView);

  _deletionQueue.pushFunction([this, lostEmpireTexture]() {
    vkDestroyImageView(_vkDevice, lostEmpireTexture.imageView, nullptr);
  });

  return newResourceId;
}

rhi::ResourceIdRHI VulkanRHI::uploadMesh(rhi::UploadMeshRHI const& meshInfo) {
  std::size_t const bufferSize = meshInfo.meshSize;
  AllocatedBuffer stagingBuffer =
      createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  meshInfo.unpackFunc(reinterpret_cast<char*>(mappedMemory));

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Mesh& mesh = _meshesNew[newResourceId];

  mesh.vertexBuffer = createBuffer(bufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  _deletionQueue.pushFunction([this, mesh] {
    vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);
  });

  immediateSubmit(
      [this, &stagingBuffer, &mesh, bufferSize](VkCommandBuffer cmd) {
        VkBufferCopy vkBufferCopy = {};
        vkBufferCopy.srcOffset = 0;
        vkBufferCopy.dstOffset = 0;
        vkBufferCopy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1,
                        &vkBufferCopy);
      });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);

  return newResourceId;
}

rhi::ResourceIdRHI
VulkanRHI::uploadShader(rhi::UploadShaderRHI const& uploadShader) {
  std::vector<std::uint32_t> buffer(
      (uploadShader.shaderDataSize + sizeof(std::uint32_t) - 1) /
      sizeof(std::uint32_t));
  uploadShader.unpackFunc(reinterpret_cast<char*>(buffer.data()));

  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  shaderModuleCreateInfo.codeSize = uploadShader.shaderDataSize;
  shaderModuleCreateInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_vkDevice, &shaderModuleCreateInfo, nullptr,
                           &shaderModule)) {
    return rhi::rhiIdUninitialized;
  }

  rhi::ResourceIdRHI newResourceId = consumeNewResourceId();
  _shaderModules[newResourceId] = shaderModule;

  return newResourceId;
}

rhi::ResourceIdRHI
VulkanRHI::uploadMaterial(rhi::UploadMaterialRHI const& uploadMaterial) {
  PipelineBuilder& pipelineBuilder =
      _pipelineBuilders[uploadMaterial.materialType];

  VkShaderModule vertexShaderModule =
      _shaderModules[uploadMaterial.vertexShaderId];

  pipelineBuilder._vkShaderStageCreateInfo.clear();
  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            vertexShaderModule));

  VkShaderModule fragmentShaderModule =
      _shaderModules[uploadMaterial.fragmentShaderId];

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            fragmentShaderModule));

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Material& newMaterial = _materialsNew[newResourceId];
  newMaterial.vkPipelineLayout = pipelineBuilder._vkPipelineLayout;
  newMaterial.vkPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _vkDefaultRenderPass);

  Texture const& albedoTexture = _texturesNew[uploadMaterial.albedoTextureId];

  VkDescriptorImageInfo albedoTexImageInfo;
  albedoTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  albedoTexImageInfo.imageView = albedoTexture.imageView;
  albedoTexImageInfo.sampler = _vkSampler;

  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindImage(0, albedoTexImageInfo,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(newMaterial.vkDescriptorSet);

  return newResourceId;
}

VkInstance VulkanRHI::getInstance() const { return _vkInstance; }

void VulkanRHI::setSurface(VkSurfaceKHR surface) { _vkSurface = surface; }

void VulkanRHI::updateExtent(rhi::WindowExtentRHI newExtent) {

  vkDeviceWaitIdle(_vkDevice);
  _skipFrame = true;
  _windowExtent.width = newExtent.width;
  _windowExtent.height = newExtent.height;

  _swapchainDeletionQueue.flush();

  initSwapchain();
  initDefaultRenderPass();
  initFramebuffers();
  initDefaultPipelines();
}

bool VulkanRHI::loadShaderModule(char const* filePath,
                                 VkShaderModule* outShaderModule) {
  std::ifstream file{filePath, std::ios::ate | std::ios::binary};

  if (!file.is_open()) {
    return false;
  }

  std::size_t const fileSize = static_cast<std::size_t>(file.tellg());
  std::vector<std::uint32_t> buffer((fileSize + sizeof(std::uint32_t) - 1) /
                                    sizeof(std::uint32_t));

  file.seekg(0);

  file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

  file.close();

  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  shaderModuleCreateInfo.codeSize = fileSize;
  shaderModuleCreateInfo.pCode = buffer.data();

  if (vkCreateShaderModule(_vkDevice, &shaderModuleCreateInfo, nullptr,
                           outShaderModule)) {
    return false;
  }

  return true;
}

void VulkanRHI::immediateSubmit(
    std::function<void(VkCommandBuffer cmd)>&& function) {
  VkCommandBufferBeginInfo commandBufferBeginInfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(_immediateSubmitContext.vkCommandBuffer,
                                &commandBufferBeginInfo));

  function(_immediateSubmitContext.vkCommandBuffer);

  VK_CHECK(vkEndCommandBuffer(_immediateSubmitContext.vkCommandBuffer));

  VkSubmitInfo submit =
      vkinit::commandBufferSubmitInfo(&_immediateSubmitContext.vkCommandBuffer);

  VK_CHECK(vkQueueSubmit(_vkGraphicsQueue, 1, &submit,
                         _immediateSubmitContext.vkFence));
  vkWaitForFences(_vkDevice, 1, &_immediateSubmitContext.vkFence, VK_TRUE,
                  9999999999);
  vkResetFences(_vkDevice, 1, &_immediateSubmitContext.vkFence);

  VK_CHECK(
      vkResetCommandPool(_vkDevice, _immediateSubmitContext.vkCommandPool, 0));
}

void VulkanRHI::loadTexture(std::string_view textureName,
                            std::filesystem::path const& texturePath) {
  Texture& lostEmpireTexture = _loadedTextures[std::string{textureName}];
  bool const lostEmpImageLoaded =
      loadImage(texturePath.c_str(), lostEmpireTexture.image);
  assert(lostEmpImageLoaded);

  VkImageViewCreateInfo lostEmpImgViewCreateInfo = vkinit::imageViewCreateInfo(
      lostEmpireTexture.image.vkImage, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT);

  vkCreateImageView(_vkDevice, &lostEmpImgViewCreateInfo, nullptr,
                    &lostEmpireTexture.imageView);

  _deletionQueue.pushFunction([this, lostEmpireTexture]() {
    vkDestroyImageView(_vkDevice, lostEmpireTexture.imageView, nullptr);
  });
}

void VulkanRHI::loadTextures() {
  loadTexture("default", {"assets/default-texture.obstex"});
  loadTexture("lost-empire", {"assets/lost_empire-RGBA.obstex"});
}

void VulkanRHI::loadMeshes() {
  Mesh& triangleMesh = _meshes["triangle"];
  triangleMesh.vertices.resize(3);
  triangleMesh.vertices[0].position = {1.0f, 1.0f, 0.0f};
  triangleMesh.vertices[1].position = {-1.0f, 1.0f, 0.0f};
  triangleMesh.vertices[2].position = {0.0f, -1.0f, 0.0f};

  triangleMesh.vertices[0].color = {0.0f, 1.0f, 0.0f};
  triangleMesh.vertices[1].color = {0.0f, 1.0f, 0.0f};
  triangleMesh.vertices[2].color = {0.7f, 0.5f, 0.1f};

  uploadMesh(triangleMesh);

  Mesh& monkeyMesh = _meshes["monkey"];
  monkeyMesh.load("assets/monkey_smooth.obsmesh");
  uploadMesh(monkeyMesh);

  Mesh& lostEmpire = _meshes["lost-empire"];
  lostEmpire.load("assets/lost_empire.obsmesh");
  uploadMesh(lostEmpire);
}

void VulkanRHI::uploadMesh(Mesh& mesh) {
  size_t const bufferSize =
      mesh.vertices.size() * sizeof(decltype(mesh.vertices)::value_type);

  AllocatedBuffer stagingBuffer =
      createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  std::memcpy(mappedMemory, mesh.vertices.data(), bufferSize);

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  mesh.vertexBuffer = createBuffer(bufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  _deletionQueue.pushFunction([this, mesh] {
    vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);
  });

  immediateSubmit(
      [this, &stagingBuffer, &mesh, bufferSize](VkCommandBuffer cmd) {
        VkBufferCopy vkBufferCopy = {};
        vkBufferCopy.srcOffset = 0;
        vkBufferCopy.dstOffset = 0;
        vkBufferCopy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1,
                        &vkBufferCopy);
      });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);
}

FrameData& VulkanRHI::getCurrentFrameData() {
  std::size_t const currentFrameDataInd = _frameNumber % frameOverlap;
  return _frameDataArray[currentFrameDataInd];
}

Material* VulkanRHI::createMaterial(VkPipeline pipeline,
                                    VkPipelineLayout pipelineLayout,
                                    std::string const& name,
                                    std::string const& albedoTexName) {
  Material mat;
  mat.vkPipeline = pipeline;
  mat.vkPipelineLayout = pipelineLayout;

  auto const albedoTexIter = _loadedTextures.find(albedoTexName);

  assert(albedoTexIter != _loadedTextures.cend() && "Error: Missing texture");

  VkDescriptorImageInfo albedoTexDescriptorImgInfo = {};
  albedoTexDescriptorImgInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  albedoTexDescriptorImgInfo.imageView = albedoTexIter->second.imageView;
  albedoTexDescriptorImgInfo.sampler = _vkSampler;
  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindImage(0, albedoTexDescriptorImgInfo,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(mat.vkDescriptorSet, _vkTexturedMaterialDescriptorSetLayout);

  Material& result = (_materials[name] = mat);
  return &result;
}

Material* VulkanRHI::getMaterial(std::string const& name) {
  auto const matIter = _materials.find(name);

  if (matIter == _materials.cend()) {
    return nullptr;
  }

  return &matIter->second;
}

Mesh* VulkanRHI::getMesh(std::string const& name) {
  auto const meshIter = _meshes.find(name);

  if (meshIter == _meshes.cend()) {
    return nullptr;
  }

  return &meshIter->second;
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

bool VulkanRHI::loadImage(char const* filePath,
                          AllocatedImage& outAllocatedImage) {
  asset::Asset textureAsset;
  asset::TextureAssetInfo textureAssetInfo;

  bool const textureAssetLoaded =
      asset::loadFromFile(filePath, textureAsset) &&
      asset::readTextureAssetInfo(textureAsset, textureAssetInfo);

  if (!textureAssetLoaded) {
    OBS_LOG_ERR("Failed to load texture asset: " + std::string(filePath));
    return false;
  }

  AllocatedBuffer stagingBuffer = createBuffer(
      textureAssetInfo.unpackedSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,

      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  asset::unpackAsset(textureAssetInfo, textureAsset.binaryBlob.data(),
                     textureAsset.binaryBlob.size(),
                     reinterpret_cast<char*>(mappedMemory));

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  AllocatedImage newImage;
  VkImageUsageFlags const imageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkExtent3D extent = {};
  extent.width = textureAssetInfo.width;
  extent.height = textureAssetInfo.height;
  extent.depth = 1;
  VkFormat const format = VK_FORMAT_R8G8B8A8_SRGB;

  VkImageCreateInfo vkImgCreateInfo =
      vkinit::imageCreateInfo(imageUsageFlags, extent, format);

  VmaAllocationCreateInfo imgAllocationCreateInfo = {};
  imgAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  vmaCreateImage(_vmaAllocator, &vkImgCreateInfo, &imgAllocationCreateInfo,
                 &newImage.vkImage, &newImage.allocation, nullptr);

  immediateSubmit([this, &extent, &newImage,
                   &stagingBuffer](VkCommandBuffer cmd) {
    VkImageMemoryBarrier vkImgBarrierToTransfer = {};
    vkImgBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImgBarrierToTransfer.pNext = nullptr;

    vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
    vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImgBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImgBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImgBarrierToTransfer.image = newImage.vkImage;

    VkImageSubresourceRange& vkImgSubresourceRangeToTransfer =
        vkImgBarrierToTransfer.subresourceRange;
    vkImgSubresourceRangeToTransfer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToTransfer.baseMipLevel = 0;
    vkImgSubresourceRangeToTransfer.levelCount = 1;
    vkImgSubresourceRangeToTransfer.baseArrayLayer = 0;
    vkImgSubresourceRangeToTransfer.layerCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &vkImgBarrierToTransfer);

    VkBufferImageCopy vkBufferImgCopy = {};
    vkBufferImgCopy.imageExtent = extent;
    vkBufferImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBufferImgCopy.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.vkImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &vkBufferImgCopy);

    VkImageMemoryBarrier vkImageBarrierToRead = {};
    vkImageBarrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageBarrierToRead.pNext = nullptr;

    vkImageBarrierToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImageBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkImageBarrierToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageBarrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkImageBarrierToRead.image = newImage.vkImage;
    vkImageBarrierToRead.srcQueueFamilyIndex = 0;
    vkImageBarrierToRead.dstQueueFamilyIndex = _graphicsQueueFamilyIndex;

    VkImageSubresourceRange& vkImgSubresourceRangeToRead =
        vkImageBarrierToRead.subresourceRange;
    vkImgSubresourceRangeToRead.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToRead.layerCount = 1;
    vkImgSubresourceRangeToRead.levelCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &vkImageBarrierToRead);
  });

  outAllocatedImage = newImage;
  _deletionQueue.pushFunction([this, newImage]() {
    vmaDestroyImage(_vmaAllocator, newImage.vkImage, newImage.allocation);
  });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);

  return true;
}

rhi::ResourceIdRHI VulkanRHI::consumeNewResourceId() {
  return _nextResourceId++;
}
