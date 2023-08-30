#pragma once

#include <glm/vec3.hpp>

#include <variant>

namespace obsidian::core {

struct DirectionalLight {
  glm::vec3 direction = {1.0f, 0.0f, 0.0f};
  glm::vec3 color = {1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
};

using Light = std::variant<DirectionalLight>;

} /*namespace obsidian::core*/
