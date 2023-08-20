#pragma once

#include <glm/vec3.hpp>

namespace obsidian::editor {

struct SceneData {
  glm::vec3 ambientColor = {0.05f, 0.05f, 0.05f};
  glm::vec3 sunlightDirection = {-1.0f, -1.0f, -1.f};
  glm::vec3 sunlightColor = {1.0f, 1.0f, 1.0f};
};

struct DataContext {
  SceneData sceneData;
};

} /*namespace obsidian::editor */
