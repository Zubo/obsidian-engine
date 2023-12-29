#pragma once

#include <obsidian/core/light_types.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <vector>

namespace obsidian::serialization {

struct GameObjectData {
  std::string gameObjectName;
  std::vector<std::string> materialPaths;
  std::string meshPath;
  std::optional<core::DirectionalLight> directionalLight;
  std::optional<core::Spotlight> spotlight;
  std::optional<float> envMapRadius;
  glm::vec3 position = {};
  glm::vec3 euler = {};
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};
  std::vector<GameObjectData> children;
};

bool serializeGameObject(GameObjectData const& gameObject,
                         nlohmann::json& outJson);

bool deserializeGameObject(nlohmann::json const& gameObjectJson,
                           GameObjectData& outGameObjectData);

} /*namespace obsidian::serialization */
