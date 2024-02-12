#include <obsidian/core/texture_format.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace obsidian::vk_rhi {

ImmediateSubmitContext::~ImmediateSubmitContext() {
  if (initialized) {
    vkDestroyCommandPool(device, vkCommandPool, nullptr);
    vkDestroyFence(device, vkFence, nullptr);
    initialized = false;
  }
}

ResourceTransferContext::~ResourceTransferContext() {
  if (initialized) {
    for (auto& pool : queueCommandPools) {
      vkDestroyCommandPool(device, pool.second, nullptr);
    }

    queueCommandPools.clear();
    initialized = false;
  }
}

VkFormat getVkTextureFormat(core::TextureFormat format) {
  switch (format) {
  case core::TextureFormat::R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case core::TextureFormat::R8G8B8A8_LINEAR:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case core::TextureFormat::R32G32_SFLOAT:
    return VK_FORMAT_R32G32_SFLOAT;
  default:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

glm::vec3 getUpVectorForLookAt(glm::vec3 direction) {
  assert(glm::length(direction) > 0.0f);
  constexpr glm::vec3 upVector = {0.0f, 1.0f, 0.0f};
  constexpr glm::vec3 leftVector = {1.0f, 0.0f, 0.0f};
  constexpr float epsilon = 0.0001f;

  // If up vector is aligned with direction, we have to fallback to left vector
  // for projection matrix to be valid
  if (std::abs(glm::dot(glm::normalize(direction), upVector)) <
      1.0f - epsilon) {
    return upVector;
  } else {
    return leftVector;
  }
}

GPUCameraData getDirectionalLightCameraData(glm::vec3 direction,
                                            glm::vec3 mainCameraPos) {
  GPUCameraData gpuCameraData;

  gpuCameraData.view = glm::lookAt(mainCameraPos, mainCameraPos + direction,
                                   getUpVectorForLookAt(direction));
  gpuCameraData.proj = glm::ortho(-50.f, 50.f, -50.f, 50.f, -100.f, 100.f);
  gpuCameraData.proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  gpuCameraData.proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
                       glm::translate(glm::vec3{0.f, 0.f, 1.f}) *
                       gpuCameraData.proj;

  gpuCameraData.viewProj = gpuCameraData.proj * gpuCameraData.view;

  return gpuCameraData;
}

GPUCameraData getSpotlightCameraData(glm::vec3 const& position,
                                     glm::vec3 const& direction,
                                     float fadeoutAngleRad) {
  GPUCameraData gpuCameraData;

  gpuCameraData.view = glm::lookAt(position, position + direction,
                                   getUpVectorForLookAt(direction));
  gpuCameraData.proj =
      glm::perspective(2 * fadeoutAngleRad, 1.0f, 0.1f, 200.0f);
  gpuCameraData.proj[1][1] *= -1;

  // Map NDC from [-1, 1] to [0, 1]
  gpuCameraData.proj = glm::scale(glm::vec3{1.f, 1.f, 0.5f}) *
                       glm::translate(glm::vec3{0.f, 0.f, 1.f}) *
                       gpuCameraData.proj;

  gpuCameraData.viewProj = gpuCameraData.proj * gpuCameraData.view;

  return gpuCameraData;
}

} /*namespace obsidian::vk_rhi*/
