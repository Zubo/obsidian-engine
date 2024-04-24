#include <obsidian/core/shapes.hpp>
#include <obsidian/core/utils/aabb.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_frame_data.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <mutex>
#include <numeric>

using namespace obsidian;
using namespace obsidian::vk_rhi;

namespace obsidian::vk_rhi {

constexpr VkClearColorValue environmentColor{0.0f, 0.3f, 0.9f, 1.0f};

struct DrawPassParams {
  FrameData currentFrameData;
  GPUCameraData cameraData;
  std::size_t frameInd;
  VkViewport viewport;
  VkRect2D scissor;
};

} // namespace obsidian::vk_rhi

template <bool ascending> // if false, then descending
auto getSortByDistanceFunc(glm::mat4 viewProj) {
  return [viewProj](auto const& dc1, auto const& dc2) {
    glm::vec4 const topCorner1 =
        viewProj * dc1.model * glm::vec4{dc1.mesh->aabb.topCorner, 1.0f};
    glm::vec4 const bottomCorner1 =
        viewProj * dc1.model * glm::vec4{dc1.mesh->aabb.bottomCorner, 1.0f};
    float const minDist1 = std::min(topCorner1.w, bottomCorner1.w);

    glm::vec4 const topCorner2 =
        viewProj * dc2.model * glm::vec4{dc2.mesh->aabb.topCorner, 1.0f};
    glm::vec4 const bottomCorner2 =
        viewProj * dc2.model * glm::vec4{dc2.mesh->aabb.bottomCorner, 1.0f};
    float const minDist2 = std::min(topCorner2.w, bottomCorner2.w);

    return ascending ? (minDist1 < minDist2) : (minDist1 > minDist2);
  };
}

void VulkanRHI::depthPrepass(DrawPassParams const& params) {
  ZoneScoped;

  VkClearValue depthClearValue;
  depthClearValue.depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo depthPassBeginInfo = {};
  depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  depthPassBeginInfo.pNext = nullptr;
  depthPassBeginInfo.renderPass = _depthRenderPass.vkRenderPass;
  depthPassBeginInfo.renderArea.offset = {0, 0};
  depthPassBeginInfo.clearValueCount = 1;
  depthPassBeginInfo.pClearValues = &depthClearValue;

  depthPassBeginInfo.renderArea.extent = {_vkbSwapchain.extent.width,
                                          _vkbSwapchain.extent.height};
  depthPassBeginInfo.framebuffer =
      _frameDataArray[params.frameInd].vkDepthPrepassFramebuffer.vkFramebuffer;

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  uploadBufferData(params.frameInd, params.cameraData, _cameraBuffer,
                   _graphicsQueueFamilyIndex);

  vkCmdBeginRenderPass(cmd, &depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  std::vector<std::uint32_t> const depthPassDynamicOffsets = {
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUSceneData))),
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData)))};

  VertexInputSpec const depthPrepassInputSpec = {true, false, false, false,
                                                 false};

  drawNoMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                  params.cameraData, _vkDepthPrepassPipeline,
                  _vkDepthPipelineLayout, depthPassDynamicOffsets,
                  _depthPrepassDescriptorSet, depthPrepassInputSpec,
                  params.viewport, params.scissor);

  vkCmdEndRenderPass(cmd);

  // transfer depth prepass
  VkImageMemoryBarrier depthPrepassAttachmentBarrier =
      vkinit::layoutImageBarrier(
          params.currentFrameData.vkDepthPrepassFramebuffer.depthBufferImage
              .vkImage,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
  depthPrepassAttachmentBarrier.srcAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthPrepassAttachmentBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthPrepassAttachmentBarrier);

  VkImageMemoryBarrier depthPrepassShaderReadBarrier =
      vkinit::layoutImageBarrier(
          params.currentFrameData.depthPassResultShaderReadImage.vkImage,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_ASPECT_DEPTH_BIT);
  depthPrepassShaderReadBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  depthPrepassShaderReadBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthPrepassShaderReadBarrier);

  VkImageCopy vkImageCopy = {};
  vkImageCopy.extent = {_vkbSwapchain.extent.width, _vkbSwapchain.extent.height,
                        1};
  vkImageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  vkImageCopy.srcSubresource.baseArrayLayer = 0;
  vkImageCopy.srcSubresource.layerCount = 1;
  vkImageCopy.srcSubresource.mipLevel = 0;
  vkImageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  vkImageCopy.dstSubresource.baseArrayLayer = 0;
  vkImageCopy.dstSubresource.layerCount = 1;
  vkImageCopy.dstSubresource.mipLevel = 0;

  vkCmdCopyImage(cmd,
                 params.currentFrameData.vkDepthPrepassFramebuffer
                     .depthBufferImage.vkImage,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 params.currentFrameData.depthPassResultShaderReadImage.vkImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkImageCopy);

  depthPrepassAttachmentBarrier.oldLayout =
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  depthPrepassAttachmentBarrier.newLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depthPrepassAttachmentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  depthPrepassAttachmentBarrier.dstAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &depthPrepassAttachmentBarrier);

  depthPrepassShaderReadBarrier.oldLayout =
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  depthPrepassShaderReadBarrier.newLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthPrepassShaderReadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  depthPrepassShaderReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthPrepassShaderReadBarrier);
}

void VulkanRHI::ssaoPass(DrawPassParams const& params) {
  ZoneScoped;

  uploadBufferData(params.frameInd, params.cameraData, _cameraBuffer,
                   _graphicsQueueFamilyIndex);

  VkRenderPassBeginInfo ssaoRenderPassBeginInfo = {};
  ssaoRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ssaoRenderPassBeginInfo.pNext = nullptr;
  ssaoRenderPassBeginInfo.renderPass = _ssaoRenderPass.vkRenderPass;
  ssaoRenderPassBeginInfo.framebuffer =
      params.currentFrameData.vkSsaoFramebuffer.vkFramebuffer;
  ssaoRenderPassBeginInfo.renderArea.offset = {0, 0};
  ssaoRenderPassBeginInfo.renderArea.extent = getSsaoExtent();
  std::array<VkClearValue, 2> ssaoClearValues = {};
  ssaoClearValues[0].color.float32[0] = 128.0f;
  ssaoClearValues[1].depthStencil.depth = 1.0f;
  ssaoRenderPassBeginInfo.clearValueCount = ssaoClearValues.size();
  ssaoRenderPassBeginInfo.pClearValues = ssaoClearValues.data();

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  vkCmdBeginRenderPass(cmd, &ssaoRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  VkViewport const viewport = getSsaoViewport();
  VkRect2D const scissor = getSsaoScissor();

  std::vector<std::uint32_t> const ssaoDynamicOffsets{
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUSceneData))),
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData)))};

  VertexInputSpec const ssaoVertInputSpec = {true, true, false, true, false};

  drawNoMaterials(cmd, _ssaoDrawCallQueue.data(), _ssaoDrawCallQueue.size(),
                  params.cameraData, _vkSsaoPipeline, _vkSsaoPipelineLayout,
                  ssaoDynamicOffsets,
                  params.currentFrameData.vkSsaoRenderPassDescriptorSet,
                  ssaoVertInputSpec, viewport, scissor);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier ssaoImageMemoryBarrier = vkinit::layoutImageBarrier(
      params.currentFrameData.vkSsaoFramebuffer.colorBufferImage.vkImage,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
  ssaoImageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  ssaoImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &ssaoImageMemoryBarrier);
}

void VulkanRHI::ssaoPostProcessingPass(DrawPassParams const& params) {
  VkRenderPassBeginInfo ssaoPostProcessingRenderPassBeginInfo = {};
  ssaoPostProcessingRenderPassBeginInfo.sType =
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ssaoPostProcessingRenderPassBeginInfo.pNext = nullptr;

  ssaoPostProcessingRenderPassBeginInfo.renderPass =
      _postProcessingRenderPass.vkRenderPass;
  ssaoPostProcessingRenderPassBeginInfo.framebuffer =
      params.currentFrameData.vkSsaoPostProcessingFramebuffer.vkFramebuffer;
  ssaoPostProcessingRenderPassBeginInfo.renderArea.offset = {0, 0};
  ssaoPostProcessingRenderPassBeginInfo.renderArea.extent = getSsaoExtent();

  VkClearValue ssaoPostProcessingClearColorValue;
  ssaoPostProcessingClearColorValue.color.float32[0] = 0.0f;
  ssaoPostProcessingRenderPassBeginInfo.clearValueCount = 1;
  ssaoPostProcessingRenderPassBeginInfo.pClearValues =
      &ssaoPostProcessingClearColorValue;

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  vkCmdBeginRenderPass(cmd, &ssaoPostProcessingRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  glm::mat3x3 kernel;
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      kernel[i][j] = 1.0f / 9.0f;
    }
  }

  VkViewport const viewport = getSsaoViewport();
  VkRect2D const scissor = getSsaoScissor();

  drawPostProcessing(
      cmd, kernel,
      params.currentFrameData.vkSsaoPostProcessingFramebuffer.vkFramebuffer,
      params.currentFrameData.vkSsaoPostProcessingDescriptorSet, viewport,
      scissor);

  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier ssaoPostProcessingBarrier = vkinit::layoutImageBarrier(
      params.currentFrameData.vkSsaoPostProcessingFramebuffer.colorBufferImage
          .vkImage,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  ssaoPostProcessingBarrier.srcAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  ssaoPostProcessingBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &ssaoPostProcessingBarrier);
}

void VulkanRHI::shadowPasses(DrawPassParams const& params) {
  ZoneScoped;

  glm::mat4 const inverseView = glm::inverse(params.cameraData.view);
  glm::vec3 const mainCameraPos = {inverseView[3][0], inverseView[3][1],
                                   inverseView[3][2]};
  std::vector<ShadowPassParams> submittedParams =
      getSubmittedShadowPassParams(mainCameraPos);

  VertexInputSpec const shadowPassVertInputSpec = {true, false, false, false,
                                                   false};

  VkClearValue depthClearValue;
  depthClearValue.depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo depthPassBeginInfo = {};
  depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  depthPassBeginInfo.pNext = nullptr;
  depthPassBeginInfo.renderPass = _depthRenderPass.vkRenderPass;
  depthPassBeginInfo.renderArea.offset = {0, 0};
  depthPassBeginInfo.clearValueCount = 1;
  depthPassBeginInfo.pClearValues = &depthClearValue;

  depthPassBeginInfo.renderArea.extent = {shadowPassAttachmentWidth,
                                          shadowPassAttachmentHeight};
  depthPassBeginInfo.framebuffer =
      _frameDataArray[params.frameInd].vkDepthPrepassFramebuffer.vkFramebuffer;

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  for (ShadowPassParams const& shadowPass : submittedParams) {
    assert(shadowPass.shadowMapIndex >= 0);

    depthPassBeginInfo.framebuffer =
        params.currentFrameData.shadowFrameBuffers[shadowPass.shadowMapIndex]
            .vkFramebuffer;

    vkCmdBeginRenderPass(cmd, &depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    std::size_t const cameraBufferInd =
        params.frameInd * rhi::maxLightsPerDrawPass + shadowPass.shadowMapIndex;

    uploadBufferData(cameraBufferInd, shadowPass.gpuCameraData,
                     _shadowPassCameraBuffer, _graphicsQueueFamilyIndex);

    std::vector<std::uint32_t> const dynamicOffsets = {
        static_cast<std::uint32_t>(params.frameInd *
                                   getPaddedBufferSize(sizeof(GPUSceneData))),
        static_cast<std::uint32_t>(cameraBufferInd *
                                   getPaddedBufferSize(sizeof(GPUCameraData)))};

    drawNoMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                    shadowPass.gpuCameraData, _vkShadowPassPipeline,
                    _vkDepthPipelineLayout, dynamicOffsets,
                    _vkShadowPassDescriptorSet, shadowPassVertInputSpec);

    vkCmdEndRenderPass(cmd);
  }

  VkImageMemoryBarrier depthImageMemoryBarrier = vkinit::layoutImageBarrier(
      VK_NULL_HANDLE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
  depthImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  for (std::size_t i = 0; i < params.currentFrameData.shadowFrameBuffers.size();
       ++i) {
    bool const depthAttachmentUsed = i < submittedParams.size();
    depthImageMemoryBarrier.oldLayout =
        depthAttachmentUsed ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageMemoryBarrier.srcAccessMask =
        depthAttachmentUsed ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                            : VK_ACCESS_NONE;

    depthImageMemoryBarrier.image =
        params.currentFrameData.shadowFrameBuffers[i].depthBufferImage.vkImage;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &depthImageMemoryBarrier);
  }
}

void VulkanRHI::colorPass(DrawPassParams const& params, glm::vec3 ambientColor,
                          VkFramebuffer targetFramebuffer, VkExtent2D extent) {
  ZoneScoped;

  std::array<VkClearValue, 2> clearValues;
  clearValues[0].color = environmentColor;
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _mainRenderPassReuseDepth.vkRenderPass;
  vkRenderPassBeginInfo.framebuffer = targetFramebuffer;
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = extent;
  vkRenderPassBeginInfo.clearValueCount = clearValues.size();
  vkRenderPassBeginInfo.pClearValues = clearValues.data();

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  std::vector<std::uint32_t> const defaultDynamicOffsets{
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUSceneData))),
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPUCameraData))),
      static_cast<std::uint32_t>(params.frameInd *
                                 getPaddedBufferSize(sizeof(GPULightData)))};

  drawWithMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                    params.cameraData, defaultDynamicOffsets,
                    params.currentFrameData.vkMainRenderPassDescriptorSet,
                    params.viewport, params.scissor, true);

  drawWithMaterials(cmd, _transparentDrawCallQueue.data(),
                    _transparentDrawCallQueue.size(), params.cameraData,
                    defaultDynamicOffsets,
                    params.currentFrameData.vkMainRenderPassDescriptorSet,
                    params.viewport, params.scissor, true);

  vkCmdEndRenderPass(cmd);
}

void VulkanRHI::environmentMapPasses(struct DrawPassParams const& params) {
  ZoneScoped;

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  constexpr std::array<glm::vec3, 6> cubeSides = {
      glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{-1.0f, 0.0f, 0.0f},
      glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{0.0f, -1.0f, 0.0f},
      glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, -1.0f}};
  constexpr std::array<glm::vec3, 6> upVecs = {
      glm::vec3{0.0f, 1.0f, 0.0f},  glm::vec3{0.0f, 1.0f, 0.0f},
      glm::vec3{0.0f, 0.0f, -1.0f}, glm::vec3{0.0f, 0.0f, 1.0f},
      glm::vec3{0.0f, 1.0f, 0.0f},  glm::vec3{0.0f, 1.0f, 0.0f}};

  VkViewport const viewport{
      0, 0, environmentMapResolution, environmentMapResolution, 0.0f, 1.0f};
  VkRect2D const scissor{{0, 0},
                         {environmentMapResolution, environmentMapResolution}};

  for (auto& keyMap : _environmentMaps) {
    EnvironmentMap& map = keyMap.second;

    if (!map.pendingUpdate) {
      continue;
    }

    VkImageMemoryBarrier colorEnvMapBarrier = vkinit::layoutImageBarrier(
        keyMap.second.colorImage.vkImage, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &colorEnvMapBarrier);

    _envMapDescriptorSetPendingUpdate = true;

    for (std::size_t i = 0; i < 6; ++i) {
      GPUCameraData cameraData;
      cameraData.view = glm::lookAt(map.pos, map.pos + cubeSides[i], upVecs[i]);
      cameraData.proj = glm::perspective(glm::radians(90.f), 1.0f, 0.1f, 400.f);
      cameraData.proj[1][1] *= -1;
      cameraData.proj[0][0] *= -1;
      cameraData.viewProj = cameraData.proj * cameraData.view;

      uploadBufferData(6 * params.frameInd + i, cameraData, map.cameraBuffer,
                       _graphicsQueueFamilyIndex);

      std::array<VkClearValue, 2> clearValues;
      clearValues[0].color = environmentColor;
      clearValues[1].depthStencil.depth = 1.0f;

      VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
      vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      vkRenderPassBeginInfo.pNext = nullptr;
      vkRenderPassBeginInfo.renderPass = _envMapRenderPass.vkRenderPass;
      vkRenderPassBeginInfo.framebuffer = map.framebuffers[i];
      vkRenderPassBeginInfo.renderArea.offset = {0, 0};
      vkRenderPassBeginInfo.renderArea.extent = {environmentMapResolution,
                                                 environmentMapResolution};
      vkRenderPassBeginInfo.clearValueCount = clearValues.size();
      vkRenderPassBeginInfo.pClearValues = clearValues.data();

      vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo,
                           VK_SUBPASS_CONTENTS_INLINE);

      std::vector<std::uint32_t> const defaultDynamicOffsets{
          static_cast<std::uint32_t>(params.frameInd *
                                     getPaddedBufferSize(sizeof(GPUSceneData))),
          static_cast<std::uint32_t>(
              (6 * params.frameInd + i) *
              getPaddedBufferSize(sizeof(GPUCameraData))),
          static_cast<std::uint32_t>(
              params.frameInd * getPaddedBufferSize(sizeof(GPULightData)))};

      drawWithMaterials(cmd, _drawCallQueue.data(), _drawCallQueue.size(),
                        cameraData, defaultDynamicOffsets,
                        map.renderPassDescriptorSets[params.frameInd], viewport,
                        scissor, false);

      drawWithMaterials(cmd, _transparentDrawCallQueue.data(),
                        _transparentDrawCallQueue.size(), cameraData,
                        defaultDynamicOffsets,
                        map.renderPassDescriptorSets[params.frameInd], viewport,
                        scissor, false);

      vkCmdEndRenderPass(cmd);
    }

    colorEnvMapBarrier = vkinit::layoutImageBarrier(
        keyMap.second.colorImage.vkImage,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    colorEnvMapBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorEnvMapBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &colorEnvMapBarrier);

    map.pendingUpdate = false;
  }
}

void VulkanRHI::present(VkSemaphore renderSemaphore,
                        std::uint32_t swapchainImageIndex) {
  ZoneScoped;

  VkPresentInfoKHR vkPresentInfo = {};
  vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  vkPresentInfo.pNext = nullptr;

  vkPresentInfo.pSwapchains = &_vkbSwapchain.swapchain;
  vkPresentInfo.swapchainCount = 1;

  vkPresentInfo.waitSemaphoreCount = 1;
  vkPresentInfo.pWaitSemaphores = &renderSemaphore;

  vkPresentInfo.pImageIndices = &swapchainImageIndex;

  VkResult presentResult;
  {
    std::scoped_lock l{_gpuQueueMutexes.at(_graphicsQueueFamilyIndex)};
    presentResult = vkQueuePresentKHR(_gpuQueues[_graphicsQueueFamilyIndex],
                                      &vkPresentInfo);
  }

  if (presentResult != VK_ERROR_OUT_OF_DATE_KHR &&
      presentResult != VK_SUBOPTIMAL_KHR) {
    VK_CHECK(presentResult);
  }
}

void VulkanRHI::clearFrameData() {
  _submittedDirectionalLights.clear();
  _submittedSpotlights.clear();
  _drawCallQueue.clear();
  _ssaoDrawCallQueue.clear();
  _transparentDrawCallQueue.clear();
}

void VulkanRHI::draw(rhi::SceneGlobalParams const& sceneParams) {
  if (_envMapDescriptorSetPendingUpdate) {
    // skipping frame
    waitDeviceIdle();
    applyPendingEnvironmentMapUpdates();
    clearFrameData();
    return;
  }

  FrameData& currentFrameData = getCurrentFrameData();

  constexpr std::uint64_t timeoutNanoseconds = 10000000000;
  {
    ZoneScopedN("Wait For Render Fence");
    VK_CHECK(vkWaitForFences(_vkDevice, 1, &currentFrameData.vkRenderFence,
                             true, timeoutNanoseconds));
  }

  destroyUnusedResources(currentFrameData.pendingResourcesToDestroy);

  uint32_t swapchainImageIndex;
  {
    ZoneScopedN("Acquire Next Image");
    VkResult const acquireImgResult =
        vkAcquireNextImageKHR(_vkDevice, _vkbSwapchain, timeoutNanoseconds,
                              currentFrameData.vkPresentSemaphore,
                              VK_NULL_HANDLE, &swapchainImageIndex);
    if (acquireImgResult == VK_ERROR_OUT_OF_DATE_KHR) {
      applyPendingExtentUpdate();
      return;
    } else {
      VK_CHECK(vkResetFences(_vkDevice, 1, &currentFrameData.vkRenderFence));
    }
  }

  DrawPassParams params;
  params.currentFrameData = currentFrameData;
  params.cameraData = getSceneCameraData(sceneParams);
  params.frameInd = _frameNumber % frameOverlap;
  params.viewport.x = 0.0f;
  params.viewport.y = 0.0f;
  params.viewport.width = _vkbSwapchain.extent.width;
  params.viewport.height = _vkbSwapchain.extent.height;
  params.viewport.minDepth = 0.0f;
  params.viewport.maxDepth = 1.0f;
  params.scissor.offset = {0, 0};
  params.scissor.extent = _vkbSwapchain.extent;

  VkCommandBuffer cmd = params.currentFrameData.vkCommandBuffer;

  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {};
  vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkCommandBufferBeginInfo.pNext = nullptr;
  vkCommandBufferBeginInfo.pInheritanceInfo = nullptr;
  vkCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &vkCommandBufferBeginInfo));

  updateTimerBuffer(cmd);

  GPUSceneData gpuSceneData;
  gpuSceneData.ambientColor = glm::vec4(sceneParams.ambientColor, 1.0f);
  uploadBufferData(params.frameInd, gpuSceneData, _sceneDataBuffer,
                   _graphicsQueueFamilyIndex);

  uploadBufferData(params.frameInd, getGPULightData(sceneParams.cameraPos),
                   _lightDataBuffer, _graphicsQueueFamilyIndex);

  auto const sortByDistanceAscending =
      getSortByDistanceFunc<true>(params.cameraData.viewProj);
  auto const sortByDistanceDescending =
      getSortByDistanceFunc<false>(params.cameraData.viewProj);

  std::sort(_transparentDrawCallQueue.begin(), _transparentDrawCallQueue.end(),
            sortByDistanceDescending);
  std::sort(_drawCallQueue.begin(), _drawCallQueue.end(),
            sortByDistanceAscending);
  std::sort(_ssaoDrawCallQueue.begin(), _ssaoDrawCallQueue.end(),
            sortByDistanceAscending);

  depthPrepass(params);

  ssaoPass(params);

  ssaoPostProcessingPass(params);

  shadowPasses(params);

  VkImageMemoryBarrier colorPassBarrier = vkinit::layoutImageBarrier(
      _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  colorPassBarrier.srcAccessMask = VK_ACCESS_NONE;
  colorPassBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &colorPassBarrier);

  Framebuffer const& framebuffer =
      _vkSwapchainFramebuffers[swapchainImageIndex][params.frameInd];
  colorPass(params, sceneParams.ambientColor, framebuffer.vkFramebuffer,
            _vkbSwapchain.extent);

  environmentMapPasses(params);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo vkSubmitInfo = vkinit::commandBufferSubmitInfo(&cmd);

  VkPipelineStageFlags const vkPipelineStageFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  vkSubmitInfo.pWaitDstStageMask = &vkPipelineStageFlags;
  vkSubmitInfo.waitSemaphoreCount = 1;
  vkSubmitInfo.pWaitSemaphores = &params.currentFrameData.vkPresentSemaphore;

  vkSubmitInfo.signalSemaphoreCount = 1;
  vkSubmitInfo.pSignalSemaphores = &params.currentFrameData.vkRenderSemaphore;

  {
    std::scoped_lock l{_gpuQueueMutexes.at(_graphicsQueueFamilyIndex)};
    VK_CHECK(vkQueueSubmit(_gpuQueues[_graphicsQueueFamilyIndex], 1,
                           &vkSubmitInfo,
                           params.currentFrameData.vkRenderFence));
  }

  present(params.currentFrameData.vkRenderSemaphore, swapchainImageIndex);

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
    std::optional<VkRect2D> dynamicScissor, bool reusesDepth) {
  ZoneScoped;

  VkMaterial const* lastMaterial = nullptr;
  for (int i = 0; i < count; ++i) {
    ZoneScopedN("Draw Object");
    VKDrawCall const& drawCall = first[i];

    assert(drawCall.material && "Error: Missing material.");
    VkMaterial const& material = *drawCall.material;

    assert(drawCall.mesh && "Error: Missing mesh");
    Mesh const& mesh = *drawCall.mesh;

    if (!core::utils::isVisible(mesh.aabb,
                                cameraData.viewProj * drawCall.model)) {
      continue;
    }

    constexpr VkPipelineBindPoint pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    if (&material != lastMaterial) {
      vkCmdBindPipeline(cmd, pipelineBindPoint,
                        reusesDepth ? material.vkPipelineReuseDepth
                                    : material.vkPipelineEnvironmentRendering);

      if (dynamicViewport) {
        vkCmdSetViewport(cmd, 0, 1, &dynamicViewport.value());
      }

      if (dynamicScissor)
        vkCmdSetScissor(cmd, 0, 1, &dynamicScissor.value());

      VkDescriptorSet objectDescriptorSet;

      if (_objectDescriptorSets.contains(drawCall.objectResourcesId)) {
        objectDescriptorSet =
            _objectDescriptorSets.at(drawCall.objectResourcesId);
      } else {
        objectDescriptorSet = _emptyDescriptorSet;
      }

      std::array<VkDescriptorSet, 4> const descriptorSets{
          _vkGlobalDescriptorSet, drawPassDescriptorSet,
          material.vkDescriptorSet, objectDescriptorSet};
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

    VkDeviceSize const indBufferOffset =
        static_cast<VkDeviceSize>(std::accumulate(
            mesh.indexBufferSizes.cbegin(),
            mesh.indexBufferSizes.cbegin() + drawCall.indexBufferInd,
            std::size_t{0}));
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

void VulkanRHI::drawNoMaterials(
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

    VkDeviceSize const indBufferOffset =
        static_cast<VkDeviceSize>(std::accumulate(
            mesh.indexBufferSizes.cbegin(),
            mesh.indexBufferSizes.cbegin() + drawCall.indexBufferInd,
            std::size_t{0}));

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

  PostProcessingPushConstants ssaoPostProcessingPushConstants;
  ssaoPostProcessingPushConstants.kernel = kernel;

  vkCmdPushConstants(
      cmd, _vkSsaoPostProcessingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
      sizeof(PostProcessingPushConstants), &ssaoPostProcessingPushConstants);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &_postProcessingQuadBuffer.buffer, &offset);

  vkCmdDraw(cmd, 6, 1, 0, 0);
}
