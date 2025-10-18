#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

using namespace obsidian::vk_rhi;

VkVertexInputDescription
VkMesh::getVertexInputDescription(VertexInputSpec inputSpec) const {

  VkVertexInputDescription description;
  bool const bindPosition = inputSpec.position;
  bool const bindNormals = inputSpec.normals && hasNormals;
  bool const bindColors = inputSpec.colors && hasColors;
  bool const bindUV = inputSpec.UV && hasUV;
  bool const bindTangents = inputSpec.tangents && hasTangents;

  std::uint32_t const stride =
      getStride(VertexInputSpec{.position = true,
                                .normals = hasNormals,
                                .colors = hasColors,
                                .UV = hasUV,
                                .tangents = hasTangents});

  return obsidian::vk_rhi::getVertexInputDescription(
      stride, bindPosition, bindNormals, bindColors, bindUV, bindTangents);
}

obsidian::rhi::ShaderPermutationRHI::Type
VkMesh::getAttributePermutation() const {
  if (hasNormals && hasColors) {
    return rhi::ShaderPermutationRHI::base;
  }

  if (hasColors) {
    return rhi::ShaderPermutationRHI::vertexNormalColor;
  }

  if (hasUV) {
    return rhi::ShaderPermutationRHI::vertexNormalUV;
  }

  return rhi::ShaderPermutationRHI::vertexNormal;
}
