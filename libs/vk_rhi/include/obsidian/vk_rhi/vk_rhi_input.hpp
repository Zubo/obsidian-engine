#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace obsidian::vk_rhi {

struct SceneGlobalParams {
  glm::vec3 cameraPos;
  glm::vec2 cameraRotationRad;

  glm::vec3 ambientColor;
  glm::vec3 sunDirection;
  glm::vec3 sunColor;
};

} /*namespace obsidian::vk_rhi*/
