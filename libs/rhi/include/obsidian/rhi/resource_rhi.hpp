#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/core/shapes.hpp>
#include <obsidian/core/texture_format.hpp>

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

namespace obsidian::rhi {

using ResourceIdRHI = std::int64_t;
constexpr ResourceIdRHI rhiIdUninitialized = ~0;

enum class ResourceState { initial, uploading, uploaded, invalid };

struct ResourceRHI {
  ResourceIdRHI id = rhi::rhiIdUninitialized;
  std::atomic<ResourceState> state = ResourceState::initial;
  std::atomic<std::size_t> refCount = 0;
};

struct UploadTextureRHI {
  core::TextureFormat format;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t mipLevels;
  std::function<void(char*)> unpackFunc;
};

struct UploadMeshRHI {
  std::size_t vertexCount;
  std::size_t vertexBufferSize;
  std::size_t indexCount;
  std::vector<std::size_t> indexBufferSizes;
  std::function<void(char*)> unpackFunc;
  core::Box3D aabb;
  bool hasNormals;
  bool hasColors;
  bool hasUV;
  bool hasTangents;
};

struct UploadShaderRHI {
  std::size_t shaderDataSize;
  std::function<void(char*)> unpackFunc;
};

struct UploadMaterialRHI {
  core::MaterialType materialType;
  ResourceIdRHI shaderId;
  ResourceIdRHI diffuseTextureId = rhi::rhiIdUninitialized;
  ResourceIdRHI normalTextureId = rhi::rhiIdUninitialized;
  glm::vec4 ambientColor;
  glm::vec4 diffuseColor;
  glm::vec4 specularColor;
  float shininess;
  bool transparent;
  bool hasTimer;
};

struct InitResourcesRHI {
  rhi::UploadShaderRHI shadowPassShader;
  rhi::UploadShaderRHI ssaoShader;
  rhi::UploadShaderRHI postProcessingShader;
};

} /*namespace obsidian::rhi*/
