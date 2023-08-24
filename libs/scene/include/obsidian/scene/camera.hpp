#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace obsidian::scene {

struct Camera {
  glm::vec3 pos;
  glm::vec2 rotationRad;

  glm::vec3 forward() const;
  glm::vec3 right() const;
};

} /*namespace obsidian::scene */
