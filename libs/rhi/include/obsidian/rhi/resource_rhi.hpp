#pragma once

#include <obsidian/core/texture_format.hpp>

#include <cstdint>
#include <functional>

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
  std::size_t meshSize;
  std::size_t vertexCount;
  std::function<void(char*)> unpackFunc;
};

struct UploadShaderRHI {
  std::size_t shaderDataSize;
  std::function<void(char*)> unpackFunc;
};

} /*namespace obsidian::rhi*/
