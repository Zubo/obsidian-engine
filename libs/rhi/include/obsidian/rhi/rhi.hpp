#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace obsidian::rhi {

using RHIResourceId = std::int64_t;
constexpr RHIResourceId rhiIdUninitialized = ~0;

struct WindowExtentRHI {
  std::uint32_t width;
  std::uint32_t height;
};

struct SceneGlobalParams {
  glm::vec3 cameraPos;
  glm::vec2 cameraRotationRad;

  glm::vec3 ambientColor;
  glm::vec3 sunDirection;
  glm::vec3 sunColor;
};

enum class RHIBackends { vulkan = 1 };

class ISurfaceProviderRHI;

class RHI {
public:
  virtual ~RHI() = default;

  virtual void init(WindowExtentRHI extent,
                    ISurfaceProviderRHI const& surfaceProvider) = 0;

  virtual void cleanup() = 0;

  virtual void draw(rhi::SceneGlobalParams const& sceneGlobalParams) = 0;

  virtual void updateExtent(rhi::WindowExtentRHI extent) = 0;
};

class ISurfaceProviderRHI {
public:
  virtual ~ISurfaceProviderRHI() = default;

  virtual void provideSurface(RHI&) const = 0;
};

} /*namespace obsidian::rhi*/
