#pragma once

#include <obsidian/scene/scene.hpp>

#include <nlohmann/json.hpp>

namespace obsidian::scene {

bool serializeScene(SceneState const& sceneData, nlohmann::json& outJson);

} /*namespace obsidian::scene*/
