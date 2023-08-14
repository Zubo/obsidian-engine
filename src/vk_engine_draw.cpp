#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include <vk_check.hpp>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>

void VulkanEngine::draw() {
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

  VkClearValue clearValues[2];
  float flash = 1.0f; // std::abs(std::sin(_frameNumber / 10.0f));
  clearValues[0].color = {{0.0f, 0.0f, flash, 1.0f}};
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _vkRenderPass;
  vkRenderPassBeginInfo.framebuffer = _vkFramebuffers[swapchainImageIndex];
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = WindowExtent;
  vkRenderPassBeginInfo.clearValueCount = 2;
  vkRenderPassBeginInfo.pClearValues = clearValues;

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
}

void VulkanEngine::drawObjects(VkCommandBuffer cmd, RenderObject* first,
                               int count) {
  ZoneScoped;
  glm::mat4 view = glm::mat4{1.f};
  view = glm::rotate(view, -_cameraRotationRad.x, {1.f, 0.f, 0.f});
  view = glm::rotate(view, -_cameraRotationRad.y, {0.f, 1.f, 0.f});
  view = glm::translate(view, -_cameraPos);
  glm::mat4 projection = glm::perspective(
      glm::radians(60.f),
      static_cast<float>(WindowExtent.width) / WindowExtent.height, 0.1f,
      200.f);
  projection[1][1] *= -1;
  glm::mat4 const viewProjection = projection * view;

  GPUCameraData gpuCameraData;
  gpuCameraData.view = view;
  gpuCameraData.proj = projection;
  gpuCameraData.viewProj = projection * view;

  FrameData& currentFrameData = getCurrentFrameData();

  std::size_t const frameInd = _frameNumber % frameOverlap;

  void* data = nullptr;

  VK_CHECK(vmaMapMemory(_vmaAllocator, _cameraBuffer.allocation, &data));

  char* const dstGPUCameraData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUCameraData));

  std::memcpy(dstGPUCameraData, &gpuCameraData, sizeof(gpuCameraData));

  vmaUnmapMemory(_vmaAllocator, _cameraBuffer.allocation);

  GPUSceneData gpuSceneData;
  gpuSceneData.ambientColor = {0.05f, 0.05f, 0.05f, 1.f};
  gpuSceneData.sunlightDirection = {glm::normalize(glm::vec3{0.3f, -1.f, 0.3f}),
                                    1.f};

  gpuSceneData.sunlightColor = glm::vec4(1.5f, 1.5f, 1.5f, 1.0f);

  VK_CHECK(vmaMapMemory(_vmaAllocator, _sceneDataBuffer.allocation,
                        reinterpret_cast<void**>(&data)));

  char* const dstGPUSceneData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUSceneData));

  std::memcpy(dstGPUSceneData, &gpuSceneData, sizeof(GPUSceneData));

  vmaUnmapMemory(_vmaAllocator, _sceneDataBuffer.allocation);

  vmaMapMemory(_vmaAllocator, currentFrameData.objectDataBuffer.allocation,
               &data);

  GPUObjectData* objectData = reinterpret_cast<GPUObjectData*>(data);

  for (int i = 0; i < _renderObjects.size(); ++i) {
    objectData[i].modelMat = _renderObjects[i].transformMatrix;
  }

  vmaUnmapMemory(_vmaAllocator, currentFrameData.objectDataBuffer.allocation);

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
      std::uint32_t const offsets[] = {
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUCameraData))),
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUSceneData)))};
      vkCmdBindPipeline(cmd, pipelineBindPoint, material.vkPipeline);

      VkDescriptorSet const descriptorSets[] = {
          _globalDescriptorSet, currentFrameData.objectDataDescriptorSet};
      vkCmdBindDescriptorSets(cmd, pipelineBindPoint, _vkMeshPipelineLayout, 0,
                              2, descriptorSets, 2, offsets);
    }

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);

    vkCmdDraw(cmd, mesh.vertices.size(), 1, 0, i);
  }
}
