#pragma once

#include <glm/vec3.hpp>

#include <variant>

namespace obsidian::core {

struct DirectionalLight {
  glm::vec3 direction = {1.0f, 0.0f, 0.0f};
  glm::vec3 color = {1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
};

struct Spotlight {
  glm::vec3 direction;
  glm::vec3 position;
  glm::vec3 color;
  float intensity = 1.0f;
  float cutoffAngleRad = 3.14f / 4;
};

using Light = std::variant<DirectionalLight, Spotlight>;

} /*namespace obsidian::core*/
