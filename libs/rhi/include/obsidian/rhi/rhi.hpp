#pragma once

#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/task/task_executor.hpp>

#include <cstdint>

namespace obsidian::rhi {

struct WindowExtentRHI {
  std::uint32_t width;
  std::uint32_t height;
};

enum class RHIBackends { vulkan = 1 };

constexpr std::size_t maxLightsPerDrawPass = 8;

class ISurfaceProviderRHI;

class RHI {
public:
  virtual ~RHI() = default;

  virtual void init(WindowExtentRHI extent,
                    ISurfaceProviderRHI const& surfaceProvider,
                    task::TaskExecutor& taskExecutor) = 0;

  virtual void initResources(InitResourcesRHI const& initResources) = 0;

  virtual void waitDeviceIdle() const = 0;

  virtual void cleanup() = 0;

  virtual void draw(SceneGlobalParams const& sceneGlobalParams) = 0;

  virtual void updateExtent(WindowExtentRHI extent) = 0;

  virtual ResourceRHI& initTextureResource() = 0;
  virtual void uploadTexture(ResourceIdRHI id,
                             UploadTextureRHI uploadTexture) = 0;
  virtual void releaseTexture(ResourceIdRHI id) = 0;

  virtual ResourceRHI& initMeshResource() = 0;
  virtual void uploadMesh(ResourceIdRHI id, UploadMeshRHI uploadMesh) = 0;
  virtual void releaseMesh(ResourceIdRHI id) = 0;

  virtual ResourceRHI& initShaderResource() = 0;
  virtual void uploadShader(ResourceIdRHI id, UploadShaderRHI uploadShader) = 0;
  virtual void releaseShader(ResourceIdRHI) = 0;

  virtual ResourceRHI& initMaterialResource() = 0;
  virtual void uploadMaterial(ResourceIdRHI id,
                              UploadMaterialRHI uploadMaterial) = 0;
  virtual void releaseMaterial(ResourceIdRHI) = 0;

  virtual void submitDrawCall(DrawCall const& drawCall) = 0;

  virtual void submitLight(LightSubmitParams const& light) = 0;

  virtual ResourceIdRHI createEnvironmentMap(glm::vec3 pos, float radius) = 0;
  virtual void destroyEnvMap(ResourceIdRHI envMapId) = 0;
  virtual void updateEnvironmentMap(ResourceIdRHI envMapId, glm::vec3 pos,
                                    float radius) = 0;
};

class ISurfaceProviderRHI {
public:
  virtual ~ISurfaceProviderRHI() = default;

  virtual void provideSurface(RHI&) const = 0;
};

} /*namespace obsidian::rhi*/
