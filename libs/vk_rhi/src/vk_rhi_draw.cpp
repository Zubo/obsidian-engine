#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstring>

using namespace obsidian::vk_rhi;

void VulkanRHI::draw(rhi::SceneGlobalParams const& sceneParams) {
  if (_skipFrame) {
    _skipFrame = false;
    return;
  }

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

  VkCommandBuffer cmd = currentFrameData.vkCommandBuffer;

  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {};
  vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkCommandBufferBeginInfo.pNext = nullptr;
  vkCommandBufferBeginInfo.pInheritanceInfo = nullptr;
  vkCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &vkCommandBufferBeginInfo));

  VkClearValue depthClearValue;
  depthClearValue.depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo depthPassBeginInfo = {};
  depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  depthPassBeginInfo.pNext = nullptr;
  depthPassBeginInfo.renderPass = _vkDepthRenderPass;
  depthPassBeginInfo.renderArea.offset = {0, 0};
  depthPassBeginInfo.clearValueCount = 1;
  depthPassBeginInfo.pClearValues = &depthClearValue;

  // Depth prepass:
  std::size_t const frameInd = _frameNumber % frameOverlap;

  depthPassBeginInfo.renderArea.extent = {_windowExtent.width,
                                          _windowExtent.height};
  depthPassBeginInfo.framebuffer =
      _frameDataArray[frameInd].vkDepthPrepassFramebuffer;

  vkCmdBeginRenderPass(cmd, &depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  GPUCameraData sceneCameraData = getSceneCameraData(sceneParams);

  drawDepthPass(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                _vkDepthPrepassPipeline, _depthPrepassGlobalDescriptorSet,
                _cameraBuffer, frameInd, sceneCameraData);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier depthImageMemoryBarrier = {};
  depthImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  depthImageMemoryBarrier.pNext = nullptr;
  depthImageMemoryBarrier.oldLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthImageMemoryBarrier.image = currentFrameData.depthPrepassImage.vkImage;
  depthImageMemoryBarrier.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_DEPTH_BIT;
  depthImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
  depthImageMemoryBarrier.subresourceRange.levelCount = 1;
  depthImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
  depthImageMemoryBarrier.subresourceRange.layerCount = 1;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthImageMemoryBarrier);

  // Shadow passes:
  std::vector<ShadowPassParams> submittedParams =
      getSubmittedShadowPassParams();

  depthPassBeginInfo.renderArea.extent = {shadowPassAttachmentWidth,
                                          shadowPassAttachmentHeight};

  for (ShadowPassParams const& shadowPass : submittedParams) {
    assert(shadowPass.shadowMapIndex >= 0);

    depthPassBeginInfo.framebuffer =
        currentFrameData.shadowFrameBuffers[shadowPass.shadowMapIndex];

    vkCmdBeginRenderPass(cmd, &depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    std::size_t const cameraBufferInd =
        frameInd * rhi::maxLightsPerDrawPass + shadowPass.shadowMapIndex;
    drawDepthPass(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                  _vkShadowPassPipeline, _vkShadowPassGlobalDescriptorSet,
                  _shadowPassCameraBuffer, cameraBufferInd,
                  shadowPass.gpuCameraData);

    vkCmdEndRenderPass(cmd);
  }

  for (std::size_t i = 0; i < currentFrameData.shadowMapImages.size(); ++i) {
    bool const depthAttachmentUsed = i < submittedParams.size();
    depthImageMemoryBarrier.oldLayout =
        depthAttachmentUsed ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageMemoryBarrier.image = currentFrameData.shadowMapImages[i].vkImage;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &depthImageMemoryBarrier);
  }

  // Color pass:
  std::array<VkClearValue, 2> clearValues;
  clearValues[0].color = {{0.0f, 0.0f, 1.0f, 1.0f}};
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _vkDefaultRenderPass;
  vkRenderPassBeginInfo.framebuffer = _vkFramebuffers[swapchainImageIndex];
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = _windowExtent;
  vkRenderPassBeginInfo.clearValueCount = clearValues.size();
  vkRenderPassBeginInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  drawObjects(cmd, _drawCallQueue.data(), _drawCallQueue.size(), sceneParams);

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

  VkResult const presentResult =
      vkQueuePresentKHR(_vkGraphicsQueue, &vkPresentInfo);

  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
      presentResult == VK_SUBOPTIMAL_KHR) {
  } else {
    VK_CHECK(presentResult);
  }

  _submittedDirectionalLights.clear();
  _submittedSpotlights.clear();
  _drawCallQueue.clear();
  ++_frameNumber;

  FrameMark;
}

void VulkanRHI::drawObjects(VkCommandBuffer cmd, VKDrawCall* first, int count,
                            rhi::SceneGlobalParams const& sceneParams) {
  ZoneScoped;
  GPUCameraData gpuCameraData = getSceneCameraData(sceneParams);

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

  GPUSceneData gpuSceneData;
  gpuSceneData.ambientColor = glm::vec4(sceneParams.ambientColor, 1.0f);

  std::memcpy(dstGPUSceneData, &gpuSceneData, sizeof(GPUSceneData));

  vmaUnmapMemory(_vmaAllocator, _sceneDataBuffer.allocation);

  GPULightData gpuLightData = getGPULightData();

  void* lightDataBuffer;
  VK_CHECK(vmaMapMemory(_vmaAllocator, _lightDataBuffer.allocation,
                        &lightDataBuffer));

  char* const dstGPULightData =
      reinterpret_cast<char*>(lightDataBuffer) +
      frameInd * getPaddedBufferSize(sizeof(GPULightData));

  std::memcpy(dstGPULightData, &gpuLightData, sizeof(GPULightData));

  vmaUnmapMemory(_vmaAllocator, _lightDataBuffer.allocation);

  FrameData& currentFrameData = getCurrentFrameData();

  Material const* lastMaterial;
  for (int i = 0; i < count; ++i) {
    ZoneScopedN("Draw Object");
    VKDrawCall const& drawCall = first[i];

    assert(drawCall.material && "Error: Missing material.");
    Material const& material = *drawCall.material;

    assert(drawCall.mesh && "Error: Missing mesh");
    Mesh const& mesh = *drawCall.mesh;

    constexpr VkPipelineBindPoint pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    if (&material != lastMaterial) {
      std::array<std::uint32_t, 3> const offsets{
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUCameraData))),
          static_cast<std::uint32_t>(frameInd *
                                     getPaddedBufferSize(sizeof(GPUSceneData))),
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPULightData)))};
      vkCmdBindPipeline(cmd, pipelineBindPoint, material.vkPipeline);

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
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, drawCall.material->vkPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &drawCall.model);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
  }
}

void VulkanRHI::drawDepthPass(VkCommandBuffer cmd, VKDrawCall* first, int count,
                              VkPipeline pipeline,
                              VkDescriptorSet globalDescriptorSet,
                              AllocatedBuffer const& cameraBuffer,
                              std::size_t cameraDataInd,
                              GPUCameraData const& gpuCameraData) {
  ZoneScoped;

  void* data;
  VK_CHECK(vmaMapMemory(_vmaAllocator, cameraBuffer.allocation, &data));

  assert(data);

  std::uint32_t const gpuCameraDataDynamicOffset =
      cameraDataInd * getPaddedBufferSize(sizeof(GPUCameraData));
  char* const shadowPassGpuCameraDataBegin =
      static_cast<char*>(data) + gpuCameraDataDynamicOffset;

  std::memcpy(shadowPassGpuCameraDataBegin, &gpuCameraData,
              sizeof(GPUCameraData));

  vmaUnmapMemory(_vmaAllocator, cameraBuffer.allocation);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  std::array<VkDescriptorSet, 4> shadowPassDescriptorSets = {
      globalDescriptorSet, _emptyDescriptorSet, _emptyDescriptorSet,
      _emptyDescriptorSet};

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vkDepthPipelineLayout, 0,
      shadowPassDescriptorSets.size(), shadowPassDescriptorSets.data(), 1,
      &gpuCameraDataDynamicOffset);

  for (std::size_t i = 0; i < count; ++i) {
    VKDrawCall& drawCall = first[i];
    assert(drawCall.mesh && "Error: Missing mesh");

    Mesh& mesh = *drawCall.mesh;

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, drawCall.material->vkPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &drawCall.model);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
  }
}
