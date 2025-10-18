#include <obsidian/core/texture_format.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace obsidian::vk_rhi {

VkVertexInputDescription getVertexInputDescriptionForPermutation(
    rhi::ShaderPermutationRHI::Type permutation, bool isPbr) {
  constexpr bool hasPosition = true;
  constexpr bool hasNormals = true;
  bool const hasUV =
      isPbr || (permutation == rhi::ShaderPermutationRHI::base ||
                permutation == rhi::ShaderPermutationRHI::vertexNormalUV);
  bool const hasColors =
      !isPbr && (permutation == rhi::ShaderPermutationRHI::base ||
                 permutation == rhi::ShaderPermutationRHI::vertexNormalColor);

  bool const hasTangents =
      isPbr || (permutation == rhi::ShaderPermutationRHI::base ||
                permutation == rhi::ShaderPermutationRHI::vertexNormalUV);

  std::uint32_t const stride =
      getStride(VertexInputSpec{.position = hasPosition,
                                .normals = hasNormals,
                                .colors = hasColors,
                                .UV = hasUV,
                                .tangents = hasTangents});

  return getVertexInputDescription(stride, hasPosition, hasNormals, hasColors,
                                   hasUV, hasTangents);
}

VkVertexInputDescription getVertexInputDescription(std::uint32_t stride,
                                                   bool hasPosition,
                                                   bool hasNormals,
                                                   bool hasColors, bool hasUV,
                                                   bool hasTangents) {
  VkVertexInputDescription description;
  std::uint32_t offset = 0;
  VkVertexInputBindingDescription& binding =
      description.bindings.emplace_back();
  binding.binding = 0;
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  binding.stride = stride;

  if (hasPosition) {

    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offset;
    description.attributes.push_back(positionAttribute);

    offset += sizeof(VertexPropertiesSpec::position);
  }

  if (hasNormals) {

    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offset;
    description.attributes.push_back(normalAttribute);

    offset += sizeof(VertexPropertiesSpec::normal);
  }

  if (hasColors) {

    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offset;
    description.attributes.push_back(colorAttribute);

    offset += sizeof(VertexPropertiesSpec::color);
  }

  if (hasUV) {

    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 3;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offset;
    description.attributes.push_back(uvAttribute);

    offset += sizeof(VertexPropertiesSpec::uv);
  }

  if (hasTangents) {

    VkVertexInputAttributeDescription tangentAttribute = {};
    tangentAttribute.binding = 0;
    tangentAttribute.location = 4;
    tangentAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    tangentAttribute.offset = offset;
    description.attributes.push_back(tangentAttribute);

    offset += sizeof(VertexPropertiesSpec::tangent);
  }

  return description;
}

ImmediateSubmitContext::~ImmediateSubmitContext() {
  if (initialized) {
    vkDestroyCommandPool(device, vkCommandPool, nullptr);
    vkDestroyFence(device, vkFence, nullptr);
    initialized = false;
  }
}

std::uint32_t getStride(VertexInputSpec const& vertInputSpec) {
  std::uint32_t stride = 0;

  if (vertInputSpec.position) {
    stride += sizeof(VertexPropertiesSpec::position);
  }

  if (vertInputSpec.normals) {
    stride += sizeof(VertexPropertiesSpec::normal);
  }

  if (vertInputSpec.colors) {
    stride += sizeof(VertexPropertiesSpec::color);
  }

  if (vertInputSpec.UV) {
    stride += sizeof(VertexPropertiesSpec::uv);
  }

  if (vertInputSpec.tangents) {
    stride += sizeof(VertexPropertiesSpec::tangent);
  }

  return stride;
}

void ResourceTransferContext::cleanup() {
  if (!initialized) {
    return;
  }

  if (cleanupFunction) {
    cleanupFunction();
    cleanupFunction = {};
  }

  for (auto const commandPoolKvp : queueCommandPools) {
    vkDestroyCommandPool(device, commandPoolKvp.second, nullptr);
  }

  queueCommandPools.clear();

  initialized = false;
}

ResourceTransferContext::~ResourceTransferContext() { cleanup(); }

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

  // If up vector is aligned with direction, we have to fallback to left
  // vector for projection matrix to be valid
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
