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
  glm::vec3 direction = {-1.0f, -1.0f, -1.0f};
  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  glm::vec3 color = {1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
  float cutoffAngleRad = 3.14f / 4;
  float fadeoutAngleRad = cutoffAngleRad + 0.1f;
  float linearAttenuation = 0.1f;
  float quadraticAttenuation = 0.01f;
};

using Light = std::variant<DirectionalLight, Spotlight>;

} /*namespace obsidian::core*/
