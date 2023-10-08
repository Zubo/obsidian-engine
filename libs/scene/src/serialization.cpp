#include "obsidian/core/light_types.hpp"
#include "obsidian/core/logging.hpp"
#include "obsidian/scene/game_object.hpp"
#include <cstring>
#include <exception>
#include <obsidian/scene/serialization.hpp>

namespace obsidian::scene {

constexpr char const* ambientColorJsonName = "ambientColor";
constexpr char const* sunDirectionJsonName = "sunDirection";
constexpr char const* sunColorJsonName = "sunColor";
constexpr char const* gameObjectsJsonName = "gameObjects";

constexpr char const* cameraJsonName = "camera";
constexpr char const* cameraPosJsonName = "pos";
constexpr char const* cameraRotationRadJsonName = "cameraRotationRad";

constexpr char const* gameObjectNameJsonName = "name";
constexpr char const* gameObjectPositionJsonName = "pos";
constexpr char const* gameObjectEulerJsonName = "euler";
constexpr char const* gameObjectScaleJsonName = "scale";
constexpr char const* gameObjectMaterialJsonName = "material";
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
constexpr char const* spotlightCutoffAngleRadJsonName = "intensity";
constexpr char const* spotlightFadeoutAngleRadJsonName = "intensity";
constexpr char const* spotlightLinearAttenuationJsonName = "intensity";
constexpr char const* spotlightQuadraticAttenuationJsonName = "intensity";

template <typename VectorType,
          std::size_t arrSize = sizeof(VectorType) / sizeof(float)>
std::array<float, arrSize> vecToArray(VectorType const& v) {
  std::array<float, arrSize> outArray;
  std::memcpy(outArray.data(), &v.x, sizeof(VectorType));
  return outArray;
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

nlohmann::json serializeGameObject(scene::GameObject const& gameObject) {
  nlohmann::json gameObjectJson;
  gameObjectJson[gameObjectNameJsonName] = gameObject.name;
  gameObjectJson[gameObjectPositionJsonName] =
      vecToArray(gameObject.getPosition());
  gameObjectJson[gameObjectEulerJsonName] = vecToArray(gameObject.getEuler());
  gameObjectJson[gameObjectScaleJsonName] = vecToArray(gameObject.getScale());

  if (gameObject.materialResource) {
    gameObjectJson[gameObjectMaterialJsonName] =
        gameObject.materialResource->getRelativePath().string();
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

bool serializeScene(SceneState const& sceneData, nlohmann::json& outJson) {
  try {
    outJson[ambientColorJsonName] = vecToArray(sceneData.ambientColor);
    outJson[sunDirectionJsonName] = vecToArray(sceneData.sunDirection);
    outJson[sunColorJsonName] = vecToArray(sceneData.sunColor);

    nlohmann::json& camera = outJson[cameraJsonName];
    camera[cameraPosJsonName] = vecToArray(sceneData.camera.pos);
    camera[cameraRotationRadJsonName] =
        vecToArray(sceneData.camera.rotationRad);

    for (GameObject const& gameObject : sceneData.gameObjects) {
      outJson[gameObjectsJsonName].push_back(serializeGameObject(gameObject));
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} // namespace obsidian::scene
