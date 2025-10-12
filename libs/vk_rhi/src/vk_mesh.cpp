#include <obsidian/vk_rhi/vk_mesh.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

using namespace obsidian::vk_rhi;

VertexInputDescription
VkMesh::getVertexInputDescription(VertexInputSpec inputSpec) const {
  VertexInputDescription description;

  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  mainBinding.stride = 0;

  if (inputSpec.bindPosition) {
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = mainBinding.stride;

    description.attributes.push_back(positionAttribute);
  }

  mainBinding.stride += sizeof(VertexPropertiesSpec::position);

  if (inputSpec.bindNormals && hasNormals) {
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = mainBinding.stride;

    description.attributes.push_back(normalAttribute);
  }

  if (hasNormals) {
    mainBinding.stride += sizeof(VertexPropertiesSpec::normal);
  }

  if (inputSpec.bindColors && hasColors) {
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = mainBinding.stride;

    description.attributes.push_back(colorAttribute);
  }

  if (hasColors) {
    mainBinding.stride += sizeof(VertexPropertiesSpec::color);
  }

  if (inputSpec.bindUV && hasUV) {
    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 3;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = mainBinding.stride;

    description.attributes.push_back(uvAttribute);
  }

  if (hasUV) {
    mainBinding.stride += sizeof(VertexPropertiesSpec::uv);
  }

  if (inputSpec.bindTangents && hasTangents) {
    VkVertexInputAttributeDescription tangentAttribute = {};
    tangentAttribute.binding = 0;
    tangentAttribute.location = 4;
    tangentAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    tangentAttribute.offset = mainBinding.stride;

    description.attributes.push_back(tangentAttribute);
  }

  if (hasTangents) {
    mainBinding.stride += sizeof(VertexPropertiesSpec::tangent);
  }

  description.bindings.push_back(mainBinding);

  return description;
}
