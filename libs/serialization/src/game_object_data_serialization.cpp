#include <obsidian/core/logging.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/serialization.hpp>

#include <nlohmann/json.hpp>

#include <exception>

using namespace obsidian;
using namespace obsidian::serialization;

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

bool serialization::serializeGameObject(GameObjectData const& gameObject,
                                        nlohmann::json& outJson) {
  try {
    outJson[gameObjectNameJsonName] = gameObject.gameObjectName;
    outJson[gameObjectPositionJsonName] = vecToArray(gameObject.position);
    outJson[gameObjectEulerJsonName] = vecToArray(gameObject.euler);
    outJson[gameObjectScaleJsonName] = vecToArray(gameObject.scale);

    if (gameObject.materialPaths.size()) {
      outJson[gameObjectMaterialsJsonName] = gameObject.materialPaths;
    }

    if (!gameObject.meshPath.empty()) {
      outJson[gameObjectMeshJsonName] = gameObject.meshPath;
    }

    if (gameObject.spotlight) {
      outJson[gameObjectSpotligthJsonName] =
          serializeSpotlight(*gameObject.spotlight);
    }

    if (gameObject.directionalLight) {
      outJson[gameObjectDirectionalLightJsonName] =
          serializeDirectionalLight(*gameObject.directionalLight);
    }

    for (GameObjectData const& child : gameObject.children) {
      nlohmann::json childJson;
      if (serializeGameObject(child, childJson)) {
        outJson[gameObjectChildrenJsonName].push_back(childJson);
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool serialization::deserializeGameObject(nlohmann::json const& gameObjectJson,
                                          GameObjectData& outGameObjectData) {
  try {
    outGameObjectData.gameObjectName = gameObjectJson[gameObjectNameJsonName];

    arrayToVector(gameObjectJson[gameObjectPositionJsonName],
                  outGameObjectData.position);

    arrayToVector(gameObjectJson[gameObjectEulerJsonName],
                  outGameObjectData.euler);

    arrayToVector(gameObjectJson[gameObjectScaleJsonName],
                  outGameObjectData.scale);

    if (gameObjectJson.contains(gameObjectMaterialsJsonName)) {
      for (auto const& materialJson :
           gameObjectJson[gameObjectMaterialsJsonName]) {
        outGameObjectData.materialPaths.push_back(materialJson);
      }
    }

    if (gameObjectJson.contains(gameObjectMeshJsonName)) {
      outGameObjectData.meshPath = gameObjectJson[gameObjectMeshJsonName];
    }

    if (gameObjectJson.contains(gameObjectSpotligthJsonName)) {
      outGameObjectData.spotlight.emplace(
          deserializeSpotlight(gameObjectJson[gameObjectSpotligthJsonName]));
    }

    if (gameObjectJson.contains(gameObjectDirectionalLightJsonName)) {
      outGameObjectData.directionalLight.emplace(deserializeDirectionalLight(
          gameObjectJson[gameObjectDirectionalLightJsonName]));
    }

    if (gameObjectJson.contains(gameObjectChildrenJsonName)) {
      for (auto const& childJson : gameObjectJson[gameObjectChildrenJsonName]) {
        GameObjectData childData;
        if (deserializeGameObject(childJson, childData)) {
          outGameObjectData.children.push_back(childData);
        }
      }
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}
