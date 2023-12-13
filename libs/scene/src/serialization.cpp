#include <obsidian/core/light_types.hpp>
#include <obsidian/core/logging.hpp>
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

GameObject instantiateGameObject(
    serialization::GameObjectData const& gameObjectData,
    runtime_resource::RuntimeResourceManager& resourceManager) {
  GameObject resultGameObject;

  resultGameObject.name = gameObjectData.gameObjectName;
  std::transform(gameObjectData.materialPaths.cbegin(),
                 gameObjectData.materialPaths.cend(),
                 std::back_inserter(resultGameObject.materialResources),
                 [&resourceManager](std::string const& path) {
                   return &resourceManager.getResource(path);
                 });

  resultGameObject.meshResource =
      &resourceManager.getResource(gameObjectData.meshPath);

  resultGameObject.directionalLight = gameObjectData.directionalLight;
  resultGameObject.spotlight = gameObjectData.spotlight;

  resultGameObject.setPosition(gameObjectData.position);
  resultGameObject.setEuler(gameObjectData.euler);
  resultGameObject.setScale(gameObjectData.scale);

  for (serialization::GameObjectData const& childData :
       gameObjectData.children) {
    resultGameObject.addChild(
        instantiateGameObject(childData, resourceManager));
  }

  return resultGameObject;
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

    for (GameObject const& gameObject : sceneState.gameObjects) {
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

bool deserializeScene(nlohmann::json const& sceneJson,
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
          outSceneState.gameObjects.push_back(
              instantiateGameObject(gameObjectData, resourceManager));
        }
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} // namespace obsidian::scene
