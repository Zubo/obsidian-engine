#include <obsidian/core/light_types.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/scene/serialization.hpp>

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

constexpr char const* gameObjectNameJsonName = "name";
constexpr char const* gameObjectPositionJsonName = "pos";
constexpr char const* gameObjectEulerJsonName = "euler";
constexpr char const* gameObjectScaleJsonName = "scale";
constexpr char const* gameObjectMaterialsJsonName = "materials";
constexpr char const* gameObjectMeshJsonName = "mesh";
constexpr char const* gameObjectSpotligthJsonName = "spotlight";
constexpr char const* gameObjectDirectionalLightJsonName = "directionalLight";
constexpr char const* gameObjectChildrenJsonName = "children";

constexpr char const* directionalLightDirectionJsonName = "direction";
constexpr char const* directionalLightColorJsonName = "color";
constexpr char const* directionalLightnIntensityJsonName = "intensity";

constexpr char const* spotlightDirectionJsonName = "direction";
constexpr char const* spotlightPositionJsonName = "position";
constexpr char const* spotlightColorJsonName = "color";
constexpr char const* spotlightIntensityJsonName = "intensity";
constexpr char const* spotlightCutoffAngleRadJsonName = "cutoffAngleRad";
constexpr char const* spotlightFadeoutAngleRadJsonName = "fadeoutAngleRad";
constexpr char const* spotlightLinearAttenuationJsonName = "linearAttenuation";
constexpr char const* spotlightQuadraticAttenuationJsonName =
    "quadraticAttenuation";

template <typename VectorType,
          std::size_t arrSize = sizeof(VectorType) / sizeof(float)>
std::array<float, arrSize> vecToArray(VectorType const& v) {
  std::array<float, arrSize> outArray;
  std::memcpy(outArray.data(), &v.x, sizeof(VectorType));
  return outArray;
}

template <typename VectorType>
VectorType arrayToVector(nlohmann::json const& arrayJson, VectorType& result) {
  constexpr std::size_t numberOfElements = sizeof(VectorType) / sizeof(float);

  for (std::size_t i = 0; i < numberOfElements; ++i) {
    result[i] = arrayJson[i];
  }

  return result;
}

nlohmann::json serializeSpotlight(core::Spotlight const& spotlight) {
  nlohmann::json spotlightJson;

  spotlightJson[spotlightDirectionJsonName] = vecToArray(spotlight.direction);
  spotlightJson[spotlightPositionJsonName] = vecToArray(spotlight.position);
  spotlightJson[spotlightColorJsonName] = vecToArray(spotlight.color);
  spotlightJson[spotlightIntensityJsonName] = spotlight.intensity;
  spotlightJson[spotlightCutoffAngleRadJsonName] = spotlight.cutoffAngleRad;
  spotlightJson[spotlightFadeoutAngleRadJsonName] = spotlight.fadeoutAngleRad;
  spotlightJson[spotlightLinearAttenuationJsonName] =
      spotlight.linearAttenuation;
  spotlightJson[spotlightQuadraticAttenuationJsonName] =
      spotlight.quadraticAttenuation;

  return spotlightJson;
}

core::Spotlight deserializeSpotlight(nlohmann::json const& spotlightJson) {
  core::Spotlight spotlight;

  arrayToVector(spotlightJson[spotlightDirectionJsonName],
                (spotlight.direction));
  arrayToVector(spotlightJson[spotlightPositionJsonName], (spotlight.position));
  arrayToVector(spotlightJson[spotlightColorJsonName], (spotlight.color));
  spotlight.intensity = spotlightJson[spotlightIntensityJsonName];
  spotlight.cutoffAngleRad = spotlightJson[spotlightCutoffAngleRadJsonName];
  spotlight.fadeoutAngleRad = spotlightJson[spotlightFadeoutAngleRadJsonName];
  spotlight.linearAttenuation =
      spotlightJson[spotlightLinearAttenuationJsonName];
  spotlight.quadraticAttenuation =
      spotlightJson[spotlightQuadraticAttenuationJsonName];

  return spotlight;
}

nlohmann::json
serializeDirectionalLight(core::DirectionalLight const& directionalLight) {
  nlohmann::json directionalLightJson;

  directionalLightJson[directionalLightDirectionJsonName] =
      vecToArray(directionalLight.direction);
  directionalLightJson[directionalLightColorJsonName] =
      vecToArray(directionalLight.color);
  directionalLightJson[directionalLightnIntensityJsonName] =
      directionalLight.intensity;

  return directionalLightJson;
}

core::DirectionalLight
deserializeDirectionalLight(nlohmann::json const& directionalLightJson) {
  core::DirectionalLight directionalLight;

  arrayToVector(directionalLightJson[directionalLightDirectionJsonName],
                directionalLight.direction);
  arrayToVector(directionalLightJson[directionalLightColorJsonName],
                directionalLight.color);
  directionalLight.intensity =
      directionalLightJson[directionalLightnIntensityJsonName];

  return directionalLight;
}

nlohmann::json serializeGameObject(scene::GameObject const& gameObject) {
  nlohmann::json gameObjectJson;
  gameObjectJson[gameObjectNameJsonName] = gameObject.name;
  gameObjectJson[gameObjectPositionJsonName] =
      vecToArray(gameObject.getPosition());
  gameObjectJson[gameObjectEulerJsonName] = vecToArray(gameObject.getEuler());
  gameObjectJson[gameObjectScaleJsonName] = vecToArray(gameObject.getScale());

  if (gameObject.materialResources.size()) {
    nlohmann::json& materialsJson = gameObjectJson[gameObjectMaterialsJsonName];
    std::transform(
        gameObject.materialResources.cbegin(),
        gameObject.materialResources.cend(), std::back_inserter(materialsJson),
        [](auto const& matRes) { return matRes->getRelativePath().string(); });
  }

  if (gameObject.meshResource) {
    gameObjectJson[gameObjectMeshJsonName] =
        gameObject.meshResource->getRelativePath().string();
  }

  if (gameObject.spotlight) {
    gameObjectJson[gameObjectSpotligthJsonName] =
        serializeSpotlight(*gameObject.spotlight);
  }

  if (gameObject.directionalLight) {
    gameObjectJson[gameObjectDirectionalLightJsonName] =
        serializeDirectionalLight(*gameObject.directionalLight);
  }

  for (GameObject const& child : gameObject.getChildren()) {
    gameObjectJson[gameObjectChildrenJsonName].push_back(
        serializeGameObject(child));
  }

  return gameObjectJson;
}

GameObject instantiateGameObject(
    GameObjectData const& gameObjectData,
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

  for (GameObjectData const& childData : gameObjectData.children) {
    resultGameObject.addChild(
        instantiateGameObject(childData, resourceManager));
  }

  return resultGameObject;
}

GameObjectData deserializeGameObject(nlohmann::json const& gameObjectJson) {
  GameObjectData resultGameObjectData;

  resultGameObjectData.gameObjectName = gameObjectJson[gameObjectNameJsonName];

  arrayToVector(gameObjectJson[gameObjectPositionJsonName],
                resultGameObjectData.position);

  arrayToVector(gameObjectJson[gameObjectEulerJsonName],
                resultGameObjectData.euler);

  arrayToVector(gameObjectJson[gameObjectScaleJsonName],
                resultGameObjectData.scale);

  if (gameObjectJson.contains(gameObjectMaterialsJsonName)) {
    for (auto const& materialJson :
         gameObjectJson[gameObjectMaterialsJsonName]) {
      resultGameObjectData.materialPaths.push_back(materialJson);
    }
  }

  if (gameObjectJson.contains(gameObjectMeshJsonName)) {
    resultGameObjectData.meshPath = gameObjectJson[gameObjectMeshJsonName];
  }

  if (gameObjectJson.contains(gameObjectSpotligthJsonName)) {
    resultGameObjectData.spotlight.emplace(
        deserializeSpotlight(gameObjectJson[gameObjectSpotligthJsonName]));
  }

  if (gameObjectJson.contains(gameObjectDirectionalLightJsonName)) {
    resultGameObjectData.directionalLight.emplace(deserializeDirectionalLight(
        gameObjectJson[gameObjectDirectionalLightJsonName]));
  }

  if (gameObjectJson.contains(gameObjectChildrenJsonName)) {
    for (auto const& childJson : gameObjectJson[gameObjectChildrenJsonName]) {
      resultGameObjectData.children.push_back(deserializeGameObject(childJson));
    }
  }

  return resultGameObjectData;
}

bool serializeScene(SceneState const& sceneState, nlohmann::json& outJson) {
  try {
    outJson[ambientColorJsonName] = vecToArray(sceneState.ambientColor);

    nlohmann::json& camera = outJson[cameraJsonName];
    camera[cameraPosJsonName] = vecToArray(sceneState.camera.pos);
    camera[cameraRotationRadJsonName] =
        vecToArray(sceneState.camera.rotationRad);

    for (GameObject const& gameObject : sceneState.gameObjects) {
      outJson[gameObjectsJsonName].push_back(serializeGameObject(gameObject));
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
    arrayToVector(sceneJson[ambientColorJsonName], outSceneState.ambientColor);
    arrayToVector(sceneJson[cameraJsonName][cameraPosJsonName],
                  outSceneState.camera.pos);
    arrayToVector(sceneJson[cameraJsonName][cameraRotationRadJsonName],
                  outSceneState.camera.rotationRad);

    if (sceneJson.contains(gameObjectsJsonName)) {

      for (nlohmann::json const& gameObjectJson :
           sceneJson[gameObjectsJsonName]) {
        outSceneState.gameObjects.push_back(instantiateGameObject(
            deserializeGameObject(gameObjectJson), resourceManager));
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} // namespace obsidian::scene
