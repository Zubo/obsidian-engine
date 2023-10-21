#pragma once

#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string_view>
#include <vector>

namespace obsidian::vk_rhi {

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
  glm::vec3 tangent;

  static VertexInputDescription
  getVertexInputDescription(bool bindPosition = true, bool bindNormals = true,
                            bool bindColors = true, bool bindUV = true,
                            bool bindTangents = true);
};

struct Mesh {
  VkDeviceSize vertexCount;
  AllocatedBuffer vertexBuffer;
  VkDeviceSize indexCount;
  AllocatedBuffer indexBuffer;
};

} /*namespace obsidian::vk_rhi*/
