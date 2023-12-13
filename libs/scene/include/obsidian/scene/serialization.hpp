#pragma once

#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/scene/scene.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace obsidian::runtime_resource {

class RuntimeResourceManager;

} /*namespace obsidian::runtime_resource*/

namespace obsidian::scene {

bool serializeScene(SceneState const& sceneState, nlohmann::json& outJson);

bool deserializeScene(nlohmann::json const& sceneJson,
                      runtime_resource::RuntimeResourceManager& resourceManager,
                      SceneState& outSceneState);

} /*namespace obsidian::scene*/
