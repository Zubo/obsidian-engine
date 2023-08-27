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

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Texture& newTexture = _textures[newResourceId];

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
                 &newTexture.image.vkImage, &newTexture.image.allocation,
                 nullptr);

  immediateSubmit([this, &extent, &newTexture,
                   &stagingBuffer](VkCommandBuffer cmd) {
    VkImageMemoryBarrier vkImgBarrierToTransfer = {};
    vkImgBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImgBarrierToTransfer.pNext = nullptr;

    vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
    vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImgBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImgBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImgBarrierToTransfer.image = newTexture.image.vkImage;

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

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.image.vkImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &vkBufferImgCopy);

    VkImageMemoryBarrier vkImageBarrierToRead = {};
    vkImageBarrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageBarrierToRead.pNext = nullptr;

    vkImageBarrierToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImageBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkImageBarrierToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageBarrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkImageBarrierToRead.image = newTexture.image.vkImage;
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

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);

  VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageViewCreateInfo(
      newTexture.image.vkImage, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT);

  vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr,
                    &newTexture.imageView);

  return newResourceId;
}

void VulkanRHI::unloadTexture(rhi::ResourceIdRHI resourceIdRHI) {
  vkDeviceWaitIdle(_vkDevice);

  Texture& tex = _textures[resourceIdRHI];

  vkDestroyImageView(_vkDevice, tex.imageView, nullptr);
  vmaDestroyImage(_vmaAllocator, tex.image.vkImage, tex.image.allocation);

  _textures.erase(resourceIdRHI);
}

rhi::ResourceIdRHI VulkanRHI::uploadMesh(rhi::UploadMeshRHI const& meshInfo) {
  AllocatedBuffer vertexStagingBuffer = createBuffer(
      meshInfo.vertexBufferSize + meshInfo.indexBufferSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, vertexStagingBuffer.allocation, &mappedMemory);

  meshInfo.unpackFunc(reinterpret_cast<char*>(mappedMemory));

  vmaUnmapMemory(_vmaAllocator, vertexStagingBuffer.allocation);

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Mesh& mesh = _meshes[newResourceId];

  mesh.vertexBuffer = createBuffer(meshInfo.vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  mesh.vertexCount = meshInfo.vertexCount;

  mesh.indexBuffer = createBuffer(meshInfo.indexBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  mesh.indexCount = meshInfo.indexCount;

  immediateSubmit(
      [this, &vertexStagingBuffer, &mesh, meshInfo](VkCommandBuffer cmd) {
        VkBufferCopy vkVertexBufferCopy = {};
        vkVertexBufferCopy.srcOffset = 0;
        vkVertexBufferCopy.dstOffset = 0;
        vkVertexBufferCopy.size = meshInfo.vertexBufferSize;
        vkCmdCopyBuffer(cmd, vertexStagingBuffer.buffer,
                        mesh.vertexBuffer.buffer, 1, &vkVertexBufferCopy);

        VkBufferCopy vkIndexBufferCopy = {};
        vkIndexBufferCopy.srcOffset = meshInfo.vertexBufferSize;
        vkIndexBufferCopy.dstOffset = 0;
        vkIndexBufferCopy.size = meshInfo.indexBufferSize;

        vkCmdCopyBuffer(cmd, vertexStagingBuffer.buffer,
                        mesh.indexBuffer.buffer, 1, &vkIndexBufferCopy);
      });

  vmaDestroyBuffer(_vmaAllocator, vertexStagingBuffer.buffer,
                   vertexStagingBuffer.allocation);

  return newResourceId;
}

void VulkanRHI::unloadMesh(rhi::ResourceIdRHI resourceIdRHI) {
  Mesh& mesh = _meshes[resourceIdRHI];

  vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                   mesh.vertexBuffer.allocation);

  vmaDestroyBuffer(_vmaAllocator, mesh.indexBuffer.buffer,
                   mesh.indexBuffer.allocation);

  _meshes.erase(resourceIdRHI);
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

void VulkanRHI::unloadShader(rhi::ResourceIdRHI resourceIdRHI) {
  VkShaderModule shader = _shaderModules[resourceIdRHI];
  vkDestroyShaderModule(_vkDevice, shader, nullptr);
}

rhi::ResourceIdRHI
VulkanRHI::uploadMaterial(rhi::UploadMaterialRHI const& uploadMaterial) {
  PipelineBuilder& pipelineBuilder =
      _pipelineBuilders[uploadMaterial.materialType];

  VkShaderModule shaderModule = _shaderModules[uploadMaterial.shaderId];

  pipelineBuilder._vkShaderStageCreateInfo.clear();
  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule));

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule));

  rhi::ResourceIdRHI const newResourceId = consumeNewResourceId();
  Material& newMaterial = _materials[newResourceId];
  newMaterial.vkPipelineLayout = pipelineBuilder._vkPipelineLayout;
  newMaterial.vkPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _vkDefaultRenderPass);

  Texture const& albedoTexture = _textures[uploadMaterial.albedoTextureId];

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

void VulkanRHI::unloadMaterial(rhi::ResourceIdRHI resourceIdRHI) {
  Material& material = _materials[resourceIdRHI];

  vkDeviceWaitIdle(_vkDevice);

  vkDestroyPipeline(_vkDevice, material.vkPipeline, nullptr);
}

void VulkanRHI::submitDrawCall(rhi::DrawCall const& drawCall) {
  VKDrawCall& vkDrawCall = _drawCallQueue.emplace_back();
  vkDrawCall.model = drawCall.transform;
  vkDrawCall.mesh = &_meshes[drawCall.meshId];
  vkDrawCall.material = &_materials[drawCall.materialId];
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
  initDefaultPipelineLayouts();
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
