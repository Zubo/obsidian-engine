#pragma once

#include <vk_types.hpp>

#include <glm/vec3.hpp>

#include <string_view>
#include <vector>

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  static VertexInputDescription getVertexInputDescription();
};

struct Mesh {
  std::vector<Vertex> _vertices;
  AllocatedBuffer _vertexBuffer;

  bool loadFromObj(char const *filePath);
};
