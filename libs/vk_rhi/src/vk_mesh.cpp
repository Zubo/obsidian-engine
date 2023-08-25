#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

using namespace obsidian::vk_rhi;

VertexInputDescription Vertex::getVertexInputDescription(bool bindPosition,
                                                         bool bindNormals,
                                                         bool bindColors,
                                                         bool bindUV) {
  VertexInputDescription description;

  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(Vertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(mainBinding);

  if (bindPosition) {
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(Vertex, position);

    description.attributes.push_back(positionAttribute);
  }

  if (bindNormals) {
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(Vertex, normal);

    description.attributes.push_back(normalAttribute);
  }

  if (bindColors) {
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    colorAttribute.offset = offsetof(Vertex, color);

    description.attributes.push_back(colorAttribute);
  }

  if (bindUV) {
    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.location = 3;
    uvAttribute.binding = 0;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offsetof(Vertex, uv);

    description.attributes.push_back(uvAttribute);
  }

  return description;
}
