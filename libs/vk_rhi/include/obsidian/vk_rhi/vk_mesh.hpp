#pragma once

#include <obsidian/core/shapes.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace obsidian::vk_rhi {

struct VertexPropertiesSpec {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
  glm::vec3 tangent;
};

struct VertexInputSpec {
  bool bindPosition = true;
  bool bindNormals = true;
  bool bindColors = true;
  bool bindUV = true;
  bool bindTangents = true;
};

struct Mesh {
  VkDeviceSize vertexCount;
  AllocatedBuffer vertexBuffer;
  VkDeviceSize indexCount;
  AllocatedBuffer indexBuffer;
  std::vector<std::size_t> indexBufferSizes;
  rhi::ResourceRHI resource;
  bool hasNormals;
  bool hasColors;
  bool hasUV;
  bool hasTangents;
  core::Box3D aabb;

  VertexInputDescription getVertexInputDescription(
      VertexInputSpec inputSpec = VertexInputSpec()) const;
};

} /*namespace obsidian::vk_rhi*/
