#include <obsidian/core/logging.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>
#include <obsidian/serialization/serialization.hpp>

namespace obsidian::serialization {

constexpr char const* ambientColorJsonName = "ambientColor";
constexpr char const* gameObjectsJsonName = "gameObjects";

constexpr char const* cameraJsonName = "camera";
constexpr char const* cameraPosJsonName = "pos";
constexpr char const* cameraRotationRadJsonName = "cameraRotationRad";

bool serializeScene(SceneData const& sceneData, nlohmann::json& outJson) {
  try {
    outJson[ambientColorJsonName] =
        serialization::vecToArray(sceneData.ambientColor);

    nlohmann::json& camera = outJson[cameraJsonName];
    camera[cameraPosJsonName] = serialization::vecToArray(sceneData.camera.pos);
    camera[cameraRotationRadJsonName] =
        serialization::vecToArray(sceneData.camera.rotationRad);

    for (auto const& gameObjectData : sceneData.gameObjects) {
      nlohmann::json gameObjectJson;
      if (serialization::serializeGameObject(gameObjectData, gameObjectJson)) {
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
                      SceneData& outSceneData) {
  try {
    serialization::arrayToVector(sceneJson[ambientColorJsonName],
                                 outSceneData.ambientColor);
    serialization::arrayToVector(sceneJson[cameraJsonName][cameraPosJsonName],
                                 outSceneData.camera.pos);
    serialization::arrayToVector(
        sceneJson[cameraJsonName][cameraRotationRadJsonName],
        outSceneData.camera.rotationRad);

    if (sceneJson.contains(gameObjectsJsonName)) {

      for (nlohmann::json const& gameObjectJson :
           sceneJson[gameObjectsJsonName]) {
        serialization::GameObjectData& gameObjectData =
            outSceneData.gameObjects.emplace_back();
        if (!serialization::deserializeGameObject(gameObjectJson,
                                                  gameObjectData)) {
          OBS_LOG_WARN("Failed to deserialize game object data: \n" +
                       gameObjectJson.dump(2));
        }
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} /*namespace obsidian::serialization*/
