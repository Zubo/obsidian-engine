#pragma once

#include <obsidian/core/light_types.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace obsidian::rhi {

struct SceneGlobalParams {
  glm::vec3 cameraPos;
  glm::vec2 cameraRotationRad;

  glm::vec3 ambientColor;
};

struct DrawCall {
  glm::mat4 transform;
  ResourceIdRHI materialId;
  ResourceIdRHI meshId;
};

using LightSubmitParams = core::Light;
using DirectionalLightParams = core::DirectionalLight;

struct DirectionalLight {
  DirectionalLightParams directionalLight;
  int assignedShadowMapInd = -1;
};

} /*namespace obsidian::rhi*/
