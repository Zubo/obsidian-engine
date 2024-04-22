#pragma once

#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace obsidian::serialization {

struct CameraData {
  glm::vec3 pos;
  glm::vec2 rotationRad;
};

struct SceneData {
  glm::vec3 ambientColor;

  CameraData camera;
  std::vector<GameObjectData> gameObjects;
};

bool serializeScene(SceneData const& sceneData, nlohmann::json& outJson);

bool deserializeScene(nlohmann::json const& sceneJson, SceneData& outSceneData);

} /*namespace obsidian::serialization*/
