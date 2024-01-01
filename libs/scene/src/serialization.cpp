#include <obsidian/core/light_types.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/scene/serialization.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/serialization.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <exception>
#include <iterator>

namespace obsidian::scene {

constexpr char const* ambientColorJsonName = "ambientColor";
constexpr char const* gameObjectsJsonName = "gameObjects";

constexpr char const* cameraJsonName = "camera";
constexpr char const* cameraPosJsonName = "pos";
constexpr char const* cameraRotationRadJsonName = "cameraRotationRad";

void populateGameObject(serialization::GameObjectData const& gameObjectData,
                        scene::GameObject& outGameObject) {
  outGameObject.setName(gameObjectData.gameObjectName);

  std::vector<std::filesystem::path> materialRelativePaths;

  std::transform(gameObjectData.materialPaths.cbegin(),
                 gameObjectData.materialPaths.cend(),
                 std::back_inserter(materialRelativePaths),
                 [](std::string const& path) { return path; });

  outGameObject.setMaterials(materialRelativePaths);

  outGameObject.setMesh(gameObjectData.meshPath);

  if (gameObjectData.directionalLight) {
    outGameObject.setDirectionalLight(*gameObjectData.directionalLight);
  }

  if (gameObjectData.spotlight) {
    outGameObject.setSpotlight(*gameObjectData.spotlight);
  }

  if (gameObjectData.envMapRadius) {
    outGameObject.setEnvironmentMap(*gameObjectData.envMapRadius);
  }

  outGameObject.setPosition(gameObjectData.position);
  outGameObject.setEuler(gameObjectData.euler);
  outGameObject.setScale(gameObjectData.scale);

  for (serialization::GameObjectData const& childData :
       gameObjectData.children) {
    GameObject& childGameObject = outGameObject.createChild();
    populateGameObject(childData, childGameObject);
  }
}

bool serializeScene(SceneState const& sceneState, nlohmann::json& outJson) {
  try {
    outJson[ambientColorJsonName] =
        serialization::vecToArray(sceneState.ambientColor);

    nlohmann::json& camera = outJson[cameraJsonName];
    camera[cameraPosJsonName] =
        serialization::vecToArray(sceneState.camera.pos);
    camera[cameraRotationRadJsonName] =
        serialization::vecToArray(sceneState.camera.rotationRad);

    for (auto const& gameObject : sceneState.gameObjects) {
      nlohmann::json gameObjectJson;
      if (serialization::serializeGameObject(gameObject.getGameObjectData(),
                                             gameObjectJson)) {
        outJson[gameObjectsJsonName].push_back(gameObjectJson);
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool deserializeScene(nlohmann::json const& sceneJson, rhi::RHI& rhi,
                      runtime_resource::RuntimeResourceManager& resourceManager,
                      SceneState& outSceneState) {
  try {
    serialization::arrayToVector(sceneJson[ambientColorJsonName],
                                 outSceneState.ambientColor);
    serialization::arrayToVector(sceneJson[cameraJsonName][cameraPosJsonName],
                                 outSceneState.camera.pos);
    serialization::arrayToVector(
        sceneJson[cameraJsonName][cameraRotationRadJsonName],
        outSceneState.camera.rotationRad);

    if (sceneJson.contains(gameObjectsJsonName)) {

      for (nlohmann::json const& gameObjectJson :
           sceneJson[gameObjectsJsonName]) {
        serialization::GameObjectData gameObjectData;
        if (serialization::deserializeGameObject(gameObjectJson,
                                                 gameObjectData)) {
          scene::GameObject& gameObject =
              outSceneState.gameObjects.emplace_back(rhi, resourceManager);
          scene::populateGameObject(gameObjectData, gameObject);
        }
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} // namespace obsidian::scene
