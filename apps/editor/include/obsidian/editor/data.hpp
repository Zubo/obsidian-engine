#pragma once

#include <glm/vec3.hpp>

namespace obsidian::editor {

struct SceneData {
  glm::vec3 ambientColor = {1.0f, 1.0f, 1.0f};
};

struct DataContext {
  SceneData sceneData;
};

} /*namespace obsidian::editor */
