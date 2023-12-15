#include <obsidian/core/utils/aabb.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <mutex>
#include <numeric>

using namespace obsidian::vk_rhi;

void VulkanRHI::draw(rhi::SceneGlobalParams const& sceneParams) {
  applyPendingExtentUpdate();

  if (_skipFrame) {
    _submittedDirectionalLights.clear();
    _submittedSpotlights.clear();
    _drawCallQueue.clear();
    _ssaoDrawCallQueue.clear();
    _transparentDrawCallQueue.clear();
    _skipFrame = false;
    return;
  }

  FrameData& currentFrameData = getCurrentFrameData();

  constexpr std::uint64_t timeoutNanoseconds = 10000000000;
  {
    ZoneScopedN("Wait For Render Fence");
    VK_CHECK(vkWaitForFences(_vkDevice, 1, &currentFrameData.vkRenderFence,
                             true, timeoutNanoseconds));
  }

  VK_CHECK(vkResetFences(_vkDevice, 1, &currentFrameData.vkRenderFence));

  destroyUnreferencedResources();

  uint32_t swapchainImageIndex;
  {
    ZoneScopedN("Acquire Next Image");
    VK_CHECK(vkAcquireNextImageKHR(_vkDevice, _vkbSwapchain, timeoutNanoseconds,
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
  depthPassBeginInfo.renderPass = _depthRenderPass.vkRenderPass;
  depthPassBeginInfo.renderArea.offset = {0, 0};
  depthPassBeginInfo.clearValueCount = 1;
  depthPassBeginInfo.pClearValues = &depthClearValue;

  // Depth prepass:
  std::size_t const frameInd = _frameNumber % frameOverlap;

  depthPassBeginInfo.renderArea.extent = {_vkbSwapchain.extent.width,
                                          _vkbSwapchain.extent.height};
  depthPassBeginInfo.framebuffer =
      _frameDataArray[frameInd].vkDepthPrepassFramebuffer.vkFramebuffer;

  vkCmdBeginRenderPass(cmd, &depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  GPUCameraData sceneCameraData = getSceneCameraData(sceneParams);

  VkViewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = _vkbSwapchain.extent.width;
  viewport.height = _vkbSwapchain.extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset = {0, 0};
  scissor.extent = _vkbSwapchain.extent;

  uploadBufferData(frameInd, sceneCameraData, _cameraBuffer);

  std::vector<std::uint32_t> const depthPassDynamicOffsets = {
      0, 0,
      static_cast<std::uint32_t>(frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData)))};

  VertexInputSpec const depthPrepassInputSpec = {true, false, false, false,
                                                 false};

  drawPassNoMaterials(
      cmd, _drawCallQueue.data(), _drawCallQueue.size(), sceneCameraData,
      _vkDepthPrepassPipeline, _vkDepthPipelineLayout, depthPassDynamicOffsets,
      _depthPrepassDescriptorSet, depthPrepassInputSpec, viewport, scissor);

  drawPassNoMaterials(cmd, _transparentDrawCallQueue.data(),
                      _transparentDrawCallQueue.size(), sceneCameraData,
                      _vkDepthPrepassPipeline, _vkDepthPipelineLayout,
                      depthPassDynamicOffsets, _depthPrepassDescriptorSet,
                      depthPrepassInputSpec, viewport, scissor);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier depthImageMemoryBarrier = vkinit::layoutImageBarrier(
      currentFrameData.vkDepthPrepassFramebuffer.depthBufferImage.vkImage,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthImageMemoryBarrier);

  // Ssao pass
  GPUCameraData gpuCameraData = getSceneCameraData(sceneParams);
  uploadBufferData(frameInd, gpuCameraData, _cameraBuffer);

  VkRenderPassBeginInfo ssaoRenderPassBeginInfo = {};
  ssaoRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ssaoRenderPassBeginInfo.pNext = nullptr;
  ssaoRenderPassBeginInfo.renderPass = _ssaoRenderPass.vkRenderPass;
  ssaoRenderPassBeginInfo.framebuffer =
      currentFrameData.vkSsaoFramebuffer.vkFramebuffer;
  ssaoRenderPassBeginInfo.renderArea.offset = {0, 0};
  ssaoRenderPassBeginInfo.renderArea.extent = _vkbSwapchain.extent;
  std::array<VkClearValue, 2> ssaoClearValues = {};
  ssaoClearValues[0].color.float32[0] = 128.0f;
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

  VertexInputSpec const ssaoVertInputSpec = {true, true, false, true, false};

  drawPassNoMaterials(cmd, _ssaoDrawCallQueue.data(), _ssaoDrawCallQueue.size(),
                      sceneCameraData, _vkSsaoPipeline, _vkSsaoPipelineLayout,
                      ssaoDynamicOffsets,
                      currentFrameData.vkSsaoRenderPassDescriptorSet,
                      ssaoVertInputSpec, viewport, scissor);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier ssaoImageMemoryBarrier = vkinit::layoutImageBarrier(
      currentFrameData.vkSsaoFramebuffer.colorBufferImage.vkImage,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &ssaoImageMemoryBarrier);

  // Ssao post processing
  VkRenderPassBeginInfo ssaoPostProcessingRenderPassBeginInfo = {};
  ssaoPostProcessingRenderPassBeginInfo.sType =
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ssaoPostProcessingRenderPassBeginInfo.pNext = nullptr;

  ssaoPostProcessingRenderPassBeginInfo.renderPass =
      _postProcessingRenderPass.vkRenderPass;
  ssaoPostProcessingRenderPassBeginInfo.framebuffer =
      currentFrameData.vkSsaoPostProcessingFramebuffer.vkFramebuffer;
  ssaoPostProcessingRenderPassBeginInfo.renderArea.offset = {0, 0};
  ssaoPostProcessingRenderPassBeginInfo.renderArea.extent =
      _vkbSwapchain.extent;

  VkClearValue ssaoPostProcessingClearColorValue;
  ssaoPostProcessingClearColorValue.color.float32[0] = 0.0f;
  ssaoPostProcessingRenderPassBeginInfo.clearValueCount = 1;
  ssaoPostProcessingRenderPassBeginInfo.pClearValues =
      &ssaoPostProcessingClearColorValue;

  vkCmdBeginRenderPass(cmd, &ssaoPostProcessingRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  glm::mat3x3 kernel;
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      kernel[i][j] = 1.0f / 9.0f;
    }
  }

  drawPostProcessing(
      cmd, kernel,
      currentFrameData.vkSsaoPostProcessingFramebuffer.vkFramebuffer,
      currentFrameData.vkSsaoPostProcessingDescriptorSet, viewport, scissor);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier ssaoPostProcessingBarrier = vkinit::layoutImageBarrier(
      currentFrameData.vkSsaoPostProcessingFramebuffer.colorBufferImage.vkImage,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &ssaoPostProcessingBarrier);

  // Shadow passes:
  std::vector<ShadowPassParams> submittedParams =
      getSubmittedShadowPassParams();

  VertexInputSpec const shadowPassVertInputSpec = {true, false, false, false,
                                                   false};

  depthPassBeginInfo.renderArea.extent = {shadowPassAttachmentWidth,
                                          shadowPassAttachmentHeight};

  for (ShadowPassParams const& shadowPass : submittedParams) {
    assert(shadowPass.shadowMapIndex >= 0);

    depthPassBeginInfo.framebuffer =
        currentFrameData.shadowFrameBuffers[shadowPass.shadowMapIndex]
            .vkFramebuffer;

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
                        shadowPass.gpuCameraData, _vkShadowPassPipeline,
                        _vkDepthPipelineLayout, dynamicOffsets,
                        _vkShadowPassDescriptorSet, shadowPassVertInputSpec);

    vkCmdEndRenderPass(cmd);
  }

  for (std::size_t i = 0; i < currentFrameData.shadowFrameBuffers.size(); ++i) {
    bool const depthAttachmentUsed = i < submittedParams.size();
    depthImageMemoryBarrier.oldLayout =
        depthAttachmentUsed ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageMemoryBarrier.image =
        currentFrameData.shadowFrameBuffers[i].depthBufferImage.vkImage;

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
  vkRenderPassBeginInfo.renderPass = _mainRenderPass.vkRenderPass;
  vkRenderPassBeginInfo.framebuffer =
      _vkSwapchainFramebuffers[swapchainImageIndex].vkFramebuffer;
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = _vkbSwapchain.extent;
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
                    gpuCameraData, defaultDynamicOffsets,
                    currentFrameData.vkMainRenderPassDescriptorSet, viewport,
                    scissor);

  drawWithMaterials(
      cmd, _transparentDrawCallQueue.data(), _transparentDrawCallQueue.size(),
      gpuCameraData, defaultDynamicOffsets,
      currentFrameData.vkMainRenderPassDescriptorSet, viewport, scissor);

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

  {
    std::scoped_lock l{_gpuQueueMutexes[_graphicsQueueFamilyIndex]};
    VK_CHECK(vkQueueSubmit(_gpuQueues[_graphicsQueueFamilyIndex], 1,
                           &vkSubmitInfo, currentFrameData.vkRenderFence));
  }

  VkPresentInfoKHR vkPresentInfo = {};
  vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  vkPresentInfo.pNext = nullptr;

  vkPresentInfo.pSwapchains = &_vkbSwapchain.swapchain;
  vkPresentInfo.swapchainCount = 1;

  vkPresentInfo.waitSemaphoreCount = 1;
  vkPresentInfo.pWaitSemaphores = &currentFrameData.vkRenderSemaphore;

  vkPresentInfo.pImageIndices = &swapchainImageIndex;

  VkResult presentResult;
  {
    std::scoped_lock l{_gpuQueueMutexes[_graphicsQueueFamilyIndex]};
    presentResult = vkQueuePresentKHR(_gpuQueues[_graphicsQueueFamilyIndex],
                                      &vkPresentInfo);
  }

  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
      presentResult == VK_SUBOPTIMAL_KHR) {
  } else {
    VK_CHECK(presentResult);
  }

  _submittedDirectionalLights.clear();
  _submittedSpotlights.clear();
  _drawCallQueue.clear();
  _transparentDrawCallQueue.clear();
  _ssaoDrawCallQueue.clear();
  ++_frameNumber;

  FrameMark;
}

void VulkanRHI::drawWithMaterials(
    VkCommandBuffer cmd, VKDrawCall* first, int count,
    GPUCameraData const& cameraData,
    std::vector<std::uint32_t> const& dynamicOffsets,
    VkDescriptorSet drawPassDescriptorSet,
    std::optional<VkViewport> dynamicViewport,
    std::optional<VkRect2D> dynamicScissor) {
  ZoneScoped;

  Material const* lastMaterial = nullptr;
  for (int i = 0; i < count; ++i) {
    ZoneScopedN("Draw Object");
    VKDrawCall const& drawCall = first[i];

    assert(drawCall.material && "Error: Missing material.");
    Material const& material = *drawCall.material;

    assert(drawCall.mesh && "Error: Missing mesh");
    Mesh const& mesh = *drawCall.mesh;

    if (!core::utils::isVisible(mesh.aabb,
                                cameraData.viewProj * drawCall.model)) {
      continue;
    }

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
      vkCmdBindDescriptorSets(cmd, pipelineBindPoint, material.vkPipelineLayout,
                              0, descriptorSets.size(), descriptorSets.data(),
                              dynamicOffsets.size(), dynamicOffsets.data());
      lastMaterial = &material;
    }

    VertexInputDescription const vertInputDescr =
        mesh.getVertexInputDescription({true, mesh.hasNormals, mesh.hasColors,
                                        mesh.hasUV, mesh.hasTangents});

    _vkCmdSetVertexInput(
        cmd, vertInputDescr.bindings.size(), vertInputDescr.bindings.data(),
        vertInputDescr.attributes.size(), vertInputDescr.attributes.data());

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);

    VkDeviceSize const indBufferOffset = std::accumulate(
        mesh.indexBufferSizes.cbegin(),
        mesh.indexBufferSizes.cbegin() + drawCall.indexBufferInd, 0);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, indBufferOffset,
                         VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(cmd, drawCall.material->vkPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &drawCall.model);
    vkCmdDrawIndexed(cmd,
                     mesh.indexBufferSizes[drawCall.indexBufferInd] /
                         sizeof(core::MeshIndexType),
                     1, 0, 0, 0);
  }
}

void VulkanRHI::drawPassNoMaterials(
    VkCommandBuffer cmd, VKDrawCall* first, int count,
    GPUCameraData const& cameraData, VkPipeline pipeline,
    VkPipelineLayout pipelineLayout,
    std::vector<std::uint32_t> const& dynamicOffsets,
    VkDescriptorSet passDescriptorSet, VertexInputSpec vertexInputSpec,
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

    if (!core::utils::isVisible(mesh.aabb,
                                cameraData.viewProj * drawCall.model)) {
      continue;
    }

    VertexInputDescription const vertInputDescr =
        mesh.getVertexInputDescription(vertexInputSpec);
    _vkCmdSetVertexInput(
        cmd, vertInputDescr.bindings.size(), vertInputDescr.bindings.data(),
        vertInputDescr.attributes.size(), vertInputDescr.attributes.data());

    VkDeviceSize const vertBufferOffset = 0;

    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer,
                           &vertBufferOffset);

    VkDeviceSize const indBufferOffset = std::accumulate(
        mesh.indexBufferSizes.cbegin(),
        mesh.indexBufferSizes.cbegin() + drawCall.indexBufferInd, 0);

    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, indBufferOffset,
                         VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshPushConstants), &drawCall.model);

    vkCmdDrawIndexed(cmd,
                     mesh.indexBufferSizes[drawCall.indexBufferInd] /
                         sizeof(core::MeshIndexType),
                     1, 0, 0, 0);
  }
}

void VulkanRHI::drawPostProcessing(VkCommandBuffer cmd,
                                   glm::mat3x3 const& kernel,
                                   VkFramebuffer frameBuffer,
                                   VkDescriptorSet passDescriptorSet,
                                   std::optional<VkViewport> dynamicViewport,
                                   std::optional<VkRect2D> dynamicScissor) {
  std::vector<VkVertexInputBindingDescription2EXT> bindings;
  VkVertexInputBindingDescription2EXT& bindingDescr = bindings.emplace_back();
  bindingDescr = {};
  bindingDescr.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
  bindingDescr.pNext = nullptr;
  bindingDescr.binding = 0;
  bindingDescr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  bindingDescr.stride = sizeof(glm::vec2);
  bindingDescr.divisor = 1;

  std::vector<VkVertexInputAttributeDescription2EXT> attributes;
  VkVertexInputAttributeDescription2EXT& attrDescr = attributes.emplace_back();
  attrDescr = {};
  attrDescr.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
  attrDescr.pNext = nullptr;
  attrDescr.location = 0;
  attrDescr.binding = 0;
  attrDescr.format = VK_FORMAT_R32G32_SFLOAT;
  attrDescr.offset = 0;

  _vkCmdSetVertexInput(cmd, bindings.size(), bindings.data(), attributes.size(),
                       attributes.data());

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _vkSsaoPostProcessingPipeline);

  if (dynamicViewport) {
    vkCmdSetViewport(cmd, 0, 1, &dynamicViewport.value());
  }

  if (dynamicScissor) {
    vkCmdSetScissor(cmd, 0, 1, &dynamicScissor.value());
  }

  std::array<VkDescriptorSet, 4> descriptorSets = {
      _emptyDescriptorSet, passDescriptorSet, _emptyDescriptorSet,
      _emptyDescriptorSet};

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vkSsaoPostProcessingPipelineLayout,
      0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

  PostProcessingpushConstants ssaoPostProcessingPushConstants;
  ssaoPostProcessingPushConstants.kernel = kernel;

  vkCmdPushConstants(
      cmd, _vkSsaoPostProcessingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
      sizeof(PostProcessingpushConstants), &ssaoPostProcessingPushConstants);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &_postProcessingQuadBuffer.buffer, &offset);

  vkCmdDraw(cmd, 6, 1, 0, 0);
}
