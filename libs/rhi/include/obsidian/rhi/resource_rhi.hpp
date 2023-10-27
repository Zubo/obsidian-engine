#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace obsidian::rhi {

using ResourceIdRHI = std::int64_t;

constexpr ResourceIdRHI rhiIdUninitialized = ~0;

struct UploadTextureRHI {
  core::TextureFormat format;
  std::uint32_t width;
  std::uint32_t height;
  std::function<void(char*)> unpackFunc;
};

struct UploadMeshRHI {
  std::size_t vertexCount;
  std::size_t vertexBufferSize;
  std::size_t indexCount;
  std::vector<std::size_t> indexBufferSizes;
  std::function<void(char*)> unpackFunc;
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
  glm::vec4 diffuseColor;
  float shininess;
  bool transparent;
};

struct InitResourcesRHI {
  rhi::UploadShaderRHI shadowPassShader;
  rhi::UploadShaderRHI ssaoShader;
  rhi::UploadShaderRHI postProcessingShader;
};

} /*namespace obsidian::rhi*/
