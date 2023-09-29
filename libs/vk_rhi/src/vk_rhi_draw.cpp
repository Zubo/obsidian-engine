#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <type_traits>
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

  VkViewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = _windowExtent.width;
  viewport.height = _windowExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset = {0, 0};
  scissor.extent = _windowExtent;

  uploadBufferData(frameInd, sceneCameraData, _cameraBuffer);

  std::vector<std::uint32_t> const depthPassDynamicOffsets = {
      0, 0,
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData)))};

  drawPassNoMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                      _vkDepthPrepassPipeline, _vkDepthPipelineLayout,
                      depthPassDynamicOffsets, _depthPrepassDescriptorSet,
                      viewport, scissor);

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

  // Ssao pass
  GPUCameraData gpuCameraData = getSceneCameraData(sceneParams);
  uploadBufferData(frameInd, gpuCameraData, _cameraBuffer);

  VkRenderPassBeginInfo ssaoRenderPassBeginInfo = {};
  ssaoRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ssaoRenderPassBeginInfo.pNext = nullptr;
  ssaoRenderPassBeginInfo.renderPass = _vkSsaoRenderPass;
  ssaoRenderPassBeginInfo.framebuffer = currentFrameData.vkSsaoFramebuffer;
  ssaoRenderPassBeginInfo.renderArea.offset = {0, 0};
  ssaoRenderPassBeginInfo.renderArea.extent = _windowExtent;
  std::array<VkClearValue, 2> ssaoClearValues = {};
  ssaoClearValues[0].color.float32[0] = 0.0f;
  ssaoClearValues[1].depthStencil.depth = 1.0f;
  ssaoRenderPassBeginInfo.clearValueCount = ssaoClearValues.size();
  ssaoRenderPassBeginInfo.pClearValues = ssaoClearValues.data();

  vkCmdBeginRenderPass(cmd, &ssaoRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  std::vector<std::uint32_t> const ssaoDynamicOffsets{
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData))),
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUSceneData)))};
  drawPassNoMaterials(
      cmd, _drawCallQueue.data(), _drawCallQueue.size(), _vkSsaoPipeline,
      _vkSsaoPipelineLayout, ssaoDynamicOffsets,
      currentFrameData.vkSsaoRenderPassDescriptorSet, viewport, scissor);

  vkCmdEndRenderPass(cmd);

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

    uploadBufferData(cameraBufferInd, shadowPass.gpuCameraData,
                     _shadowPassCameraBuffer);

    std::vector<std::uint32_t> const dynamicOffsets = {
        0, 0,
        static_cast<std::uint32_t>(cameraBufferInd *
                                   getPaddedBufferSize(sizeof(GPUCameraData)))};

    drawPassNoMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                        _vkShadowPassPipeline, _vkDepthPipelineLayout,
                        dynamicOffsets, _vkShadowPassDescriptorSet);

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

  GPUSceneData gpuSceneData;
  gpuSceneData.ambientColor = glm::vec4(sceneParams.ambientColor, 1.0f);
  uploadBufferData(frameInd, gpuSceneData, _sceneDataBuffer);

  uploadBufferData(frameInd, getGPULightData(), _lightDataBuffer);

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

  std::vector<std::uint32_t> const defaultDynamicOffsets{
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData))),
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUSceneData))),
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPULightData)))};

  drawWithMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                    _vkLitMeshPipelineLayout, defaultDynamicOffsets,
                    currentFrameData.vkDefaultRenderPassDescriptorSet, viewport,
                    scissor);

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

template <typename T>
void VulkanRHI::uploadBufferData(std::size_t const index, T const& value,
                                 AllocatedBuffer const& buffer) {
  using ValueType = std::decay_t<T>;
  void* data = nullptr;

  VK_CHECK(vmaMapMemory(_vmaAllocator, buffer.allocation, &data));

  char* const dstBufferData = reinterpret_cast<char*>(data) +
                              index * getPaddedBufferSize(sizeof(ValueType));

  std::memcpy(dstBufferData, &value, sizeof(ValueType));

  vmaUnmapMemory(_vmaAllocator, buffer.allocation);
}

void VulkanRHI::drawWithMaterials(
    VkCommandBuffer cmd, VKDrawCall* first, int count,
    VkPipelineLayout pipelineLayout,
    std::vector<std::uint32_t> const& dynamicOffsets,
    VkDescriptorSet drawPassDescriptorSet,
    std::optional<VkViewport> dynamicViewport,
    std::optional<VkRect2D> dynamicScissor) {
  ZoneScoped;

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
      vkCmdBindPipeline(cmd, pipelineBindPoint, material.vkPipeline);

      if (dynamicViewport) {
        vkCmdSetViewport(cmd, 0, 1, &dynamicViewport.value());
      }

      if (dynamicScissor)
        vkCmdSetScissor(cmd, 0, 1, &dynamicScissor.value());

      std::array<VkDescriptorSet, 4> const descriptorSets{
          _vkGlobalDescriptorSet, drawPassDescriptorSet,
          material.vkDescriptorSet, _emptyDescriptorSet};
      vkCmdBindDescriptorSets(cmd, pipelineBindPoint, pipelineLayout, 0,
                              descriptorSets.size(), descriptorSets.data(),
                              dynamicOffsets.size(), dynamicOffsets.data());
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

void VulkanRHI::drawPassNoMaterials(
    VkCommandBuffer cmd, VKDrawCall* first, int count, VkPipeline pipeline,
    VkPipelineLayout pipelineLayout,
    std::vector<std::uint32_t> const& dynamicOffsets,
    VkDescriptorSet passDescriptorSet,
    std::optional<VkViewport> dynamicViewport,
    std::optional<VkRect2D> dynamicScissor) {
  ZoneScoped;

  constexpr VkPipelineBindPoint pipelineBindPoint =
      VK_PIPELINE_BIND_POINT_GRAPHICS;

  vkCmdBindPipeline(cmd, pipelineBindPoint, pipeline);

  if (dynamicViewport) {
    vkCmdSetViewport(cmd, 0, 1, &dynamicViewport.value());
  }

  if (dynamicScissor) {
    vkCmdSetScissor(cmd, 0, 1, &dynamicScissor.value());
  }

  std::array<VkDescriptorSet, 4> descriptorSets = {
      _vkGlobalDescriptorSet, passDescriptorSet, _emptyDescriptorSet,
      _emptyDescriptorSet};

  vkCmdBindDescriptorSets(cmd, pipelineBindPoint, pipelineLayout, 0,
                          descriptorSets.size(), descriptorSets.data(),
                          dynamicOffsets.size(), dynamicOffsets.data());

  for (std::size_t i = 0; i < count; ++i) {
    VKDrawCall& drawCall = first[i];
    assert(drawCall.mesh && "Error: Missing mesh");

    Mesh& mesh = *drawCall.mesh;

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshPushConstants), &drawCall.model);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
  }
}
