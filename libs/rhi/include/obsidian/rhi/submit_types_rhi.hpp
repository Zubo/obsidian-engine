#pragma once

#include <obsidian/core/light_types.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace obsidian::rhi {

struct SceneGlobalParams {
  glm::vec3 cameraPos;
  glm::vec2 cameraRotationRad;

  glm::vec3 ambientColor;
};

struct DrawCall {
  glm::mat4 transform;
  std::vector<ResourceIdRHI> materialIds;
  ResourceIdRHI meshId;
};

using LightSubmitParams = core::Light;
using DirectionalLightParams = core::DirectionalLight;
using SpotlightParams = core::Spotlight;

struct DirectionalLight {
  DirectionalLightParams directionalLight;
  int assignedShadowMapInd = -1;
};

struct Spotlight {
  SpotlightParams spotlight;
  int assignedShadowMapInd = -1;
};

} /*namespace obsidian::rhi*/
