#include <vk_rhi/vk_check.hpp>
#include <vk_rhi/vk_initializers.hpp>
#include <vk_rhi/vk_rhi.hpp>
#include <vk_rhi/vk_types.hpp>

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <cstring>

using namespace obsidian::vk_rhi;

void VulkanRHI::draw() {
  FrameData& currentFrameData = getCurrentFrameData();

  constexpr std::uint64_t timeoutNanoseconds = 1000000000;
  {
    ZoneScopedN("Wait For Fences");
    VK_CHECK(vkWaitForFences(_vkDevice, 1, &currentFrameData.vkRenderFence,
                             true, timeoutNanoseconds));
  }
  VK_CHECK(vkResetFences(_vkDevice, 1, &currentFrameData.vkRenderFence));

  uint32_t swapchainImageIndex;
  {
    ZoneScopedN("Acquire Next Image");
    VK_CHECK(vkAcquireNextImageKHR(_vkDevice, _vkSwapchain, timeoutNanoseconds,
                                   currentFrameData.vkPresentSemaphore,
                                   VK_NULL_HANDLE, &swapchainImageIndex));
  }

  void* data;
  vmaMapMemory(_vmaAllocator, currentFrameData.vkObjectDataBuffer.allocation,
               &data);

  GPUObjectData* objectData = reinterpret_cast<GPUObjectData*>(data);

  for (int i = 0; i < _renderObjects.size(); ++i) {
    objectData[i].modelMat = _renderObjects[i].transformMatrix;
  }

  vmaUnmapMemory(_vmaAllocator, currentFrameData.vkObjectDataBuffer.allocation);

  VkCommandBuffer cmd = currentFrameData.vkCommandBuffer;

  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {};
  vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkCommandBufferBeginInfo.pNext = nullptr;
  vkCommandBufferBeginInfo.pInheritanceInfo = nullptr;
  vkCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &vkCommandBufferBeginInfo));

  std::array<VkClearValue, 2> clearValues;
  float flash = 1.0f; // std::abs(std::sin(_frameNumber / 10.0f));
  clearValues[0].color = {{0.0f, 0.0f, flash, 1.0f}};
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo vkShadowRenderPassBeginInfo = {};
  vkShadowRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkShadowRenderPassBeginInfo.pNext = nullptr;
  vkShadowRenderPassBeginInfo.renderPass = _vkShadowRenderPass;
  vkShadowRenderPassBeginInfo.framebuffer = currentFrameData.shadowFrameBuffer;
  vkShadowRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkShadowRenderPassBeginInfo.renderArea.extent = {shadowPassAttachmentWidth,
                                                   shadowPassAttachmentHeight};
  VkClearValue depthClearValue;
  depthClearValue.depthStencil.depth = 1.0f;

  vkShadowRenderPassBeginInfo.clearValueCount = 1;
  vkShadowRenderPassBeginInfo.pClearValues = &depthClearValue;

  vkCmdBeginRenderPass(cmd, &vkShadowRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  drawShadowPass(cmd, _renderObjects.data(), _renderObjects.size());

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier vkShadowMapImageMemoryBarrier = {};
  vkShadowMapImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  vkShadowMapImageMemoryBarrier.pNext = nullptr;
  vkShadowMapImageMemoryBarrier.oldLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  vkShadowMapImageMemoryBarrier.newLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkShadowMapImageMemoryBarrier.image = currentFrameData.shadowMapImage.vkImage;
  vkShadowMapImageMemoryBarrier.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_DEPTH_BIT;
  vkShadowMapImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
  vkShadowMapImageMemoryBarrier.subresourceRange.levelCount = 1;
  vkShadowMapImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
  vkShadowMapImageMemoryBarrier.subresourceRange.layerCount = 1;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &vkShadowMapImageMemoryBarrier);

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _vkRenderPass;
  vkRenderPassBeginInfo.framebuffer = _vkFramebuffers[swapchainImageIndex];
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = _windowExtent;
  vkRenderPassBeginInfo.clearValueCount = clearValues.size();
  vkRenderPassBeginInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  drawObjects(cmd, _renderObjects.data(), _renderObjects.size());

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo vkSubmitInfo = vkinit::commandBufferSubmitInfo(&cmd);

  VkPipelineStageFlags const vkPipelineStageFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  vkSubmitInfo.pWaitDstStageMask = &vkPipelineStageFlags;
  vkSubmitInfo.waitSemaphoreCount = 1;
  vkSubmitInfo.pWaitSemaphores = &currentFrameData.vkPresentSemaphore;

  vkSubmitInfo.signalSemaphoreCount = 1;
  vkSubmitInfo.pSignalSemaphores = &currentFrameData.vkRenderSemaphore;

  VK_CHECK(vkQueueSubmit(_vkGraphicsQueue, 1, &vkSubmitInfo,
                         currentFrameData.vkRenderFence));

  VkPresentInfoKHR vkPresentInfo = {};
  vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  vkPresentInfo.pNext = nullptr;

  vkPresentInfo.pSwapchains = &_vkSwapchain;
  vkPresentInfo.swapchainCount = 1;

  vkPresentInfo.waitSemaphoreCount = 1;
  vkPresentInfo.pWaitSemaphores = &currentFrameData.vkRenderSemaphore;

  vkPresentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_vkGraphicsQueue, &vkPresentInfo));

  ++_frameNumber;

  FrameMark;
}

void VulkanRHI::drawObjects(VkCommandBuffer cmd, RenderObject* first,
                            int count) {
  ZoneScoped;
  glm::mat4 view = glm::mat4{1.f};
  view = glm::rotate(view, -_cameraRotationRad.x, {1.f, 0.f, 0.f});
  view = glm::rotate(view, -_cameraRotationRad.y, {0.f, 1.f, 0.f});
  view = glm::translate(view, -_cameraPos);
  glm::mat4 proj = glm::perspective(glm::radians(60.f),
                                    static_cast<float>(_windowExtent.width) /
                                        _windowExtent.height,
                                    0.1f, 200.f);
  proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
         glm::translate(glm::vec3{0.f, 0.f, 1.f}) * proj;
  glm::mat4 const viewProjection = proj * view;

  GPUCameraData gpuCameraData;
  gpuCameraData.view = view;
  gpuCameraData.proj = proj;
  gpuCameraData.viewProj = proj * view;

  std::size_t const frameInd = _frameNumber % frameOverlap;

  void* data = nullptr;

  VK_CHECK(vmaMapMemory(_vmaAllocator, _cameraBuffer.allocation, &data));

  char* const dstGPUCameraData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUCameraData));

  std::memcpy(dstGPUCameraData, &gpuCameraData, sizeof(gpuCameraData));

  vmaUnmapMemory(_vmaAllocator, _cameraBuffer.allocation);

  VK_CHECK(vmaMapMemory(_vmaAllocator, _sceneDataBuffer.allocation,
                        reinterpret_cast<void**>(&data)));

  char* const dstGPUSceneData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUSceneData));

  std::memcpy(dstGPUSceneData, &_gpuSceneData, sizeof(GPUSceneData));

  vmaUnmapMemory(_vmaAllocator, _sceneDataBuffer.allocation);

  FrameData& currentFrameData = getCurrentFrameData();

  Material const* lastMaterial;
  for (int i = 0; i < count; ++i) {
    ZoneScopedN("Draw Object");
    RenderObject const& obj = first[i];

    assert(obj.material && "Error: Missing material.");
    Material const& material = *obj.material;

    assert(obj.mesh && "Error: Missing mesh");
    Mesh const& mesh = *obj.mesh;

    constexpr VkPipelineBindPoint pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    if (&material != lastMaterial) {
      std::array<std::uint32_t, 3> const offsets{
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUCameraData))),
          static_cast<std::uint32_t>(frameInd *
                                     getPaddedBufferSize(sizeof(GPUSceneData))),
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUCameraData)))};
      vkCmdBindPipeline(cmd, pipelineBindPoint, _vkLitMeshPipeline);

      std::array<VkDescriptorSet, 4> const descriptorSets{
          _vkGlobalDescriptorSet,
          currentFrameData.vkDefaultRenderPassDescriptorSet,
          material.vkDescriptorSet, currentFrameData.vkObjectDataDescriptorSet};
      vkCmdBindDescriptorSets(cmd, pipelineBindPoint, _vkLitMeshPipelineLayout,
                              0, descriptorSets.size(), descriptorSets.data(),
                              offsets.size(), offsets.data());
    }

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);

    vkCmdDraw(cmd, mesh.vertices.size(), 1, 0, i);
  }
}

void VulkanRHI::drawShadowPass(VkCommandBuffer cmd, RenderObject* first,
                               int count) {
  ZoneScoped;

  glm::mat4 const view = glm::lookAt(
      {}, glm::normalize(glm::vec3(_gpuSceneData.sunlightDirection)),
      {0.f, 1.f, 0.f});
  glm::mat4 proj = glm::ortho(-200.f, 200.f, -200.f, 200.f, -200.f, 200.f);
  proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
         glm::translate(glm::vec3{0.f, 0.f, 1.f}) * proj;

  GPUCameraData gpuCameraData;
  gpuCameraData.view = view;
  gpuCameraData.proj = proj;
  gpuCameraData.viewProj = proj * view;

  void* data;
  VK_CHECK(
      vmaMapMemory(_vmaAllocator, _shadowPassCameraBuffer.allocation, &data));

  assert(data);

  std::size_t const frameInd = _frameNumber % frameOverlap;

  std::uint32_t const gpuCameraDataDynamicOffset =
      frameInd * getPaddedBufferSize(sizeof(GPUCameraData));
  char* const shadowPassGpuCameraDataBegin =
      static_cast<char*>(data) + gpuCameraDataDynamicOffset;

  std::memcpy(shadowPassGpuCameraDataBegin, &gpuCameraData,
              sizeof(gpuCameraData));

  vmaUnmapMemory(_vmaAllocator, _shadowPassCameraBuffer.allocation);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _vkShadowPassPipeline);

  FrameData& currentFrameData = getCurrentFrameData();

  std::array<VkDescriptorSet, 4> shadowPassDescriptorSets = {
      _vkShadowPassGlobalDescriptorSet, _emptyDescriptorSet,
      _emptyDescriptorSet, currentFrameData.vkObjectDataDescriptorSet};

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vkShadowPassPipelineLayout, 0,
      shadowPassDescriptorSets.size(), shadowPassDescriptorSets.data(), 1,
      &gpuCameraDataDynamicOffset);

  for (std::size_t i = 0; i < count; ++i) {
    RenderObject& renderObject = first[i];
    assert(renderObject.mesh && "Error: Missing mesh");

    Mesh& mesh = *renderObject.mesh;

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);

    vkCmdDraw(cmd, mesh.vertices.size(), 1u, 0u, i);
  }
}
