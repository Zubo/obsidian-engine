#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace obsidian::vk_rhi {

VkFormat getVkTextureFormat(core::TextureFormat format) {
  switch (format) {
  case core::TextureFormat::R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  default:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

GPUCameraData getDirectionalLightCameraData(glm::vec3 direction) {
  GPUCameraData gpuCameraData;

  gpuCameraData.view =
      glm::lookAt({}, glm::normalize(direction), {0.f, 1.f, 0.f});
  gpuCameraData.proj = glm::ortho(-200.f, 200.f, -200.f, 200.f, -200.f, 200.f);
  gpuCameraData.proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  gpuCameraData.proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
                       glm::translate(glm::vec3{0.f, 0.f, 1.f}) *
                       gpuCameraData.proj;

  gpuCameraData.viewProj = gpuCameraData.proj * gpuCameraData.view;

  return gpuCameraData;
}

GPUCameraData getSpotlightCameraData(glm::vec3 direction,
                                     float cutoffAngleRad) {
  GPUCameraData gpuCameraData;

  gpuCameraData.view =
      glm::lookAt({}, glm::normalize(direction), {0.f, 1.f, 0.f});
  gpuCameraData.proj = glm::perspective(cutoffAngleRad, 1.0f, 0.1f, 300.0f);
  gpuCameraData.proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  gpuCameraData.proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
                       glm::translate(glm::vec3{0.f, 0.f, 1.f}) *
                       gpuCameraData.proj;

  gpuCameraData.viewProj = gpuCameraData.proj * gpuCameraData.view;

  return gpuCameraData;
}

} /*namespace obsidian::vk_rhi*/
