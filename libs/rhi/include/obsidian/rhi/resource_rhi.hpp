#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/core/shapes.hpp>
#include <obsidian/core/texture_format.hpp>

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <variant>
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
  obsidian::core::TextureFormat format;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t mipLevels;
  std::function<void(char*)> unpackFunc;
  char const* debugName = nullptr;
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
  char const* debugName = nullptr;
};

struct UploadShaderRHI {
  std::size_t shaderDataSize;
  std::function<void(char*)> unpackFunc;
  char const* debugName = nullptr;
};

struct UploadUnlitMaterialRHI {
  glm::vec4 color;
  ResourceIdRHI colorTextureId = rhi::rhiIdUninitialized;
};

struct UploadLitMaterialRHI {
  ResourceIdRHI diffuseTextureId = rhi::rhiIdUninitialized;
  ResourceIdRHI normalTextureId = rhi::rhiIdUninitialized;
  glm::vec4 ambientColor;
  glm::vec4 diffuseColor;
  glm::vec4 specularColor;
  float shininess;
  bool reflection;
};

struct UploadPBRMaterialRHI {
  ResourceIdRHI albedoTextureId = rhi::rhiIdUninitialized;
  ResourceIdRHI normalTextureId = rhi::rhiIdUninitialized;
  ResourceIdRHI metalnessTextureId = rhi::rhiIdUninitialized;
  ResourceIdRHI roughnessTextureId = rhi::rhiIdUninitialized;
};

using UploadMaterialSubtypeRHI =
    std::variant<UploadUnlitMaterialRHI, UploadLitMaterialRHI,
                 UploadPBRMaterialRHI>;

struct UploadMaterialRHI {
  core::MaterialType materialType;
  UploadMaterialSubtypeRHI uploadMaterialSubtype;
  ResourceIdRHI vertexShaderId;
  ResourceIdRHI fragmentShaderId;
  bool transparent;
  bool hasTimer;
  char const* debugName = nullptr;
};

class ResourceTransferRHI {
public:
  ResourceTransferRHI() = default;
  explicit ResourceTransferRHI(std::future<void> transferCompletedFuture);

  bool transferStarted() const;
  void waitCompleted() const;

private:
  std::future<void> _transferCompletedFuture;
};

struct InitResourcesRHI {
  UploadShaderRHI shadowPassVertexShader;
  UploadShaderRHI shadowPassFragmentShader;
  UploadShaderRHI ssaoVertexShader;
  UploadShaderRHI ssaoFragmentShader;
  UploadShaderRHI postProcessingVertexShader;
  UploadShaderRHI postProcessingFragmentShader;
};

} /*namespace obsidian::rhi*/
