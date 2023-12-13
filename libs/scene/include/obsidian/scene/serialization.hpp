#pragma once

#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/scene/scene.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace obsidian::runtime_resource {

class RuntimeResourceManager;

} /*namespace obsidian::runtime_resource*/

namespace obsidian::scene {

struct GameObjectData {
  std::string gameObjectName;
  std::vector<std::string> materialPaths;
  std::string meshPath;
  std::optional<core::DirectionalLight> directionalLight;
  std::optional<core::Spotlight> spotlight;
  glm::vec3 position = {};
  glm::vec3 euler = {};
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};
  std::vector<GameObjectData> children;
};

bool serializeScene(SceneState const& sceneState, nlohmann::json& outJson);

bool deserializeScene(nlohmann::json const& sceneJson,
                      runtime_resource::RuntimeResourceManager& resourceManager,
                      SceneState& outSceneState);

nlohmann::json serializeGameObject(scene::GameObject const& gameObject);

GameObjectData deserializeGameObject(nlohmann::json const& gameObjectJson);

} /*namespace obsidian::scene*/
