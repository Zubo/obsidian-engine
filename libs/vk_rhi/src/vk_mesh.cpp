#include <obsidian/vk_rhi/vk_mesh.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan_core.h>

using namespace obsidian::vk_rhi;

VertexInputDescription
Mesh::getVertexInputDescription(VertexInputSpec inputSpec) const {
  VertexInputDescription description;

  VkVertexInputBindingDescription2EXT mainBinding = {};
  mainBinding.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
  mainBinding.binding = 0;
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  mainBinding.divisor = 1;

  mainBinding.stride = 0;

  if (inputSpec.bindPosition) {
    VkVertexInputAttributeDescription2EXT positionAttribute = {};
    positionAttribute.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = mainBinding.stride;

    description.attributes.push_back(positionAttribute);
  }

  mainBinding.stride += sizeof(Vertex::position);

  if (inputSpec.bindNormals && hasNormals) {
    VkVertexInputAttributeDescription2EXT normalAttribute = {};
    normalAttribute.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = mainBinding.stride;

    description.attributes.push_back(normalAttribute);
  }

  if (hasNormals) {
    mainBinding.stride += sizeof(Vertex::normal);
  }

  if (inputSpec.bindColors && hasColors) {
    VkVertexInputAttributeDescription2EXT colorAttribute = {};
    colorAttribute.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    colorAttribute.offset = mainBinding.stride;

    description.attributes.push_back(colorAttribute);
  }

  if (hasColors) {
    mainBinding.stride += sizeof(Vertex::color);
  }

  if (inputSpec.bindUV && hasUV) {
    VkVertexInputAttributeDescription2EXT uvAttribute = {};
    uvAttribute.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    uvAttribute.binding = 0;
    uvAttribute.location = 3;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = mainBinding.stride;

    description.attributes.push_back(uvAttribute);
  }

  if (hasUV) {
    mainBinding.stride += sizeof(Vertex::uv);
  }

  if (inputSpec.bindTangents && hasTangents) {
    VkVertexInputAttributeDescription2EXT tangentAttribute = {};
    tangentAttribute.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    tangentAttribute.binding = 0;
    tangentAttribute.location = 4;
    tangentAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    tangentAttribute.offset = mainBinding.stride;

    description.attributes.push_back(tangentAttribute);
  }

  if (hasTangents) {
    mainBinding.stride += sizeof(Vertex::tangent);
  }

  description.bindings.push_back(mainBinding);

  return description;
}
