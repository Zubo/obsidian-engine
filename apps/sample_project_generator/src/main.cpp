#include "obsidian/asset/asset.hpp"
#include "obsidian/asset/asset_info.hpp"
#include "obsidian/asset/asset_io.hpp"
#include <obsidian/asset/prefab_asset_info.hpp>
#include <obsidian/asset/scene_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

int main(int argc, char const** argv) {
  using namespace obsidian;

  if (!fs::exists(SAMPLE_ASSETS_DIR)) {
    OBS_LOG_ERR("Sample asset directory missing at path " SAMPLE_ASSETS_DIR);
    return 1;
  }

  if (argc < 2) {
    OBS_LOG_ERR("Missing output project directory path parameter.");
    return 1;
  }

  project::Project project;

  if (!project.open(argv[1])) {
    OBS_LOG_ERR("Failed to open project at path " + std::string(argv[1]));
    return 1;
  }

  unsigned int nCores = std::max(std::thread::hardware_concurrency(), 2u);

  task::TaskExecutor taskExecutor;
  taskExecutor.initAndRun({{task::TaskType::general, nCores}});

  asset_converter::AssetConverter converter{taskExecutor};
  converter.setMaterialType(core::MaterialType::lit);

  fs::path const modelPath =
      fs::path{SAMPLE_ASSETS_DIR} / "SM_Deccer_Cubes_Merged_Texture_Atlas.gltf";

  if (!converter.convertAsset(modelPath, argv[1] / modelPath.stem())) {
    OBS_LOG_ERR("Failed to convert " + modelPath.string());
    return 1;
  }

  serialization::SceneData sceneData = {};
  sceneData.ambientColor = glm::vec3{0.1f, 0.1f, 0.1f};
  sceneData.camera.pos = {-8, 7, -1};
  sceneData.camera.rotationRad = {-0.65f, -1.8f};

  serialization::GameObjectData lightObject = {};
  lightObject.gameObjectName = "light";
  lightObject.directionalLight.emplace();
  lightObject.directionalLight->intensity = 10.0f;
  lightObject.rotationQuat = {
      0.865707755,
      0.416951329,
      -0.264505982,
      0.0820861608,
  };
  sceneData.gameObjects.push_back(lightObject);

  fs::path const defaultPrefabPath =
      fs::path{argv[1]} / "SM_Deccer_Cubes_Merged_Texture_AtlasCube.obspref";

  asset::Asset prefabAsset = {};
  if (!asset::loadAssetFromFile(defaultPrefabPath, prefabAsset)) {
    OBS_LOG_ERR("Failed to load prefab on path " + defaultPrefabPath.string());
    return 1;
  }

  asset::PrefabAssetInfo prefabAssetInfo;
  if (!asset::readPrefabAssetInfo(*prefabAsset.metadata, prefabAssetInfo)) {
    OBS_LOG_ERR("Failed to read prefab asset info from asset on path " +
                defaultPrefabPath.string());
    return 1;
  }

  std::string gameObjectDataString;
  gameObjectDataString.resize(prefabAssetInfo.unpackedSize);
  asset::unpackAsset(prefabAssetInfo, prefabAsset.binaryBlob.data(),
                     prefabAsset.binaryBlob.size(),
                     gameObjectDataString.data());

  nlohmann::json prefabJson;

  try {
    prefabJson = nlohmann::json::parse(gameObjectDataString);
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return 1;
  }

  serialization::GameObjectData gameObjectData = {};
  if (!serialization::deserializeGameObject(prefabJson, gameObjectData)) {
    OBS_LOG_ERR("Failed to deserialize prefab from asset on path " +
                defaultPrefabPath.string());
    return 1;
  }

  sceneData.gameObjects.push_back(gameObjectData);

  nlohmann::json sceneJson;

  if (!serialization::serializeScene(sceneData, sceneJson)) {
    OBS_LOG_ERR("Failed to serialize the scene.");
    return 1;
  }

  std::string const sceneJsonStr = sceneJson.dump();

  asset::SceneAssetInfo sceneAssetInfo;
  sceneAssetInfo.unpackedSize = sceneJsonStr.size();
  sceneAssetInfo.compressionMode = asset::CompressionMode::LZ4;

  std::vector<char> sceneJsonData;
  sceneJsonData.resize(sceneAssetInfo.unpackedSize);
  std::memcpy(sceneJsonData.data(), sceneJsonStr.data(), sceneJsonData.size());

  asset::Asset sceneAsset;

  if (!asset::packSceneAsset(sceneAssetInfo, std::move(sceneJsonData),
                             sceneAsset)) {
    OBS_LOG_ERR("Failed to pack scene asset.");
    return 1;
  }

  fs::path const saveScenePath = fs::path{argv[1]} / "scene.obsscene";

  if (!asset::saveToFile(saveScenePath, sceneAsset)) {
    OBS_LOG_ERR("Failed to save scene to path " + saveScenePath.string());
  }

  return 0;
}
