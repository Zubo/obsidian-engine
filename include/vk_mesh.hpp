#pragma once

#include <vk_engine.hpp>
#include <vk_types.hpp>

#include <glm/vec2.hpp>
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
  glm::vec2 uv;

  static VertexInputDescription getVertexInputDescription();
};

struct Mesh {
  std::vector<Vertex> vertices;
  AllocatedBuffer vertexBuffer;

  bool loadFromObj(char const* filePath);
};
