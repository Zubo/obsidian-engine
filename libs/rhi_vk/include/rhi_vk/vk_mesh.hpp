#pragma once

#include <rhi_vk/vk_engine.hpp>
#include <rhi_vk/vk_types.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string_view>
#include <vector>

namespace obsidian::rhi_vk {

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static VertexInputDescription
  getVertexInputDescription(bool bindPosition = true, bool bindNormals = true,
                            bool bindColors = true, bool bindUV = true);
};

struct Mesh {
  std::vector<Vertex> vertices;
  AllocatedBuffer vertexBuffer;

  bool loadFromObj(char const* filePath);
};

} /*namespace obsidian::rhi_vk*/
