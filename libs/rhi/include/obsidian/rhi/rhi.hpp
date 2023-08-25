#pragma once

#include <cstdint>
#include <obsidian/rhi/resource_rhi.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace obsidian::rhi {

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

struct DrawCall {
  glm::mat4 transform;
  ResourceIdRHI materialId;
  ResourceIdRHI meshId;
};

enum class RHIBackends { vulkan = 1 };

class ISurfaceProviderRHI;

class RHI {
public:
  virtual ~RHI() = default;

  virtual void init(WindowExtentRHI extent,
                    ISurfaceProviderRHI const& surfaceProvider) = 0;

  virtual void initResources(InitResourcesRHI const& initResources) = 0;

  virtual void cleanup() = 0;

  virtual void draw(rhi::SceneGlobalParams const& sceneGlobalParams) = 0;

  virtual void updateExtent(rhi::WindowExtentRHI extent) = 0;

  virtual rhi::ResourceIdRHI
  uploadTexture(UploadTextureRHI const& uploadTexture) = 0;
  virtual void unloadTexture(rhi::ResourceIdRHI) = 0;

  virtual rhi::ResourceIdRHI uploadMesh(UploadMeshRHI const& uploadMesh) = 0;
  virtual void unloadMesh(rhi::ResourceIdRHI) = 0;

  virtual rhi::ResourceIdRHI
  uploadShader(UploadShaderRHI const& uploadShader) = 0;
  virtual void unloadShader(rhi::ResourceIdRHI) = 0;

  virtual rhi::ResourceIdRHI
  uploadMaterial(UploadMaterialRHI const& uploadMaterial) = 0;
  virtual void unloadMaterial(rhi::ResourceIdRHI) = 0;

  virtual void submitDrawCall(DrawCall const& drawCall) = 0;
};

class ISurfaceProviderRHI {
public:
  virtual ~ISurfaceProviderRHI() = default;

  virtual void provideSurface(RHI&) const = 0;
};

} /*namespace obsidian::rhi*/
