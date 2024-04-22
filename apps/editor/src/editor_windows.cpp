/*
 *
 *
 *  Needs refactor
 *
 *
 */

#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/prefab_asset_info.hpp>
#include <obsidian/asset/scene_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/light_types.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/utils/path_utils.hpp>
#include <obsidian/core/utils/visitor.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/editor/item_list_data_source.hpp>
#include <obsidian/editor/settings.hpp>
#include <obsidian/globals/file_extensions.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/platform/environment.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>
#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <ImGuiFileDialog.h>
#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <glm/ext/vector_int3.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

namespace obsidian::editor {

constexpr const char* sceneWindowName = "Scene";
constexpr std::size_t maxPathSize = 256;
constexpr std::size_t maxFileNameSize = 64;
constexpr std::size_t maxGameObjectNameSize = 64;
static obsidian::project::Project project;
namespace fs = std::filesystem;

static scene::GameObject* selectedGameObj = nullptr;
static std::vector<int> selectedGameObjMats;
constexpr bool meshIncludeNone = true;
static int meshComboIndex = 0;
static asset::MeshAssetInfo selectedGameObjMeshAssetInfo;
static ItemListDataSource<fs::path> texturesInProj;
static ItemListDataSource<fs::path> shadersInProj;
static ItemListDataSource<fs::path> materialsInProj;
static ItemListDataSource<fs::path> meshesInProj;
static bool assetListDirty = false;
static std::array<char const*, 3> materialTypes = {"unlit", "lit", "pbr"};
static std::array<char const*, 4> textureTypes = {
    "Unknown", "R8G8B8A8_SRGB", "R8G8B8A8_LINEAR", "R32G32_SFLOAT"};
static bool openEngineTab = false;
scene::GameObject* pendingObjDelete = nullptr;
std::vector<fs::path> _pendingDraggedFiles;

void refreshAssetLists() {
  texturesInProj.setValues(
      project.getAllFilesWithExtension(globals::textureAssetExt));

  auto transformToStr = [](auto& dst, auto& src) {
    std::transform(src.cbegin(), src.cend(), std::back_inserter(dst),
                   [](auto const& arg) { return arg.c_str(); });
  };

  shadersInProj.setValues(
      project.getAllFilesWithExtension(globals::shaderAssetExt));

  materialsInProj.setValues(
      project.getAllFilesWithExtension(globals::materialAssetExt));

  meshesInProj.setValues(
      project.getAllFilesWithExtension(globals::meshAssetExt));
}

template <typename TCollection, typename TValue>
int indexorDefault(TCollection const& collection, TValue const& val,
                   int defaultVal) {
  auto const resultIter =
      std::find(collection.cbegin(), collection.cend(), val);

  if (resultIter == collection.cend()) {
    return defaultVal;
  }

  return std::distance(collection.cbegin(), resultIter);
}

void loadScene(char const scenePath[], scene::Scene& scene,
               ObsidianEngine& engine) {
  asset::Asset sceneAsset;
  if (!asset::loadAssetFromFile(project.getAbsolutePath(scenePath),
                                sceneAsset)) {
    OBS_LOG_ERR("Failed to load scene asset.");
    return;
  }

  asset::SceneAssetInfo sceneAssetInfo;
  if (!asset::readSceneAssetInfo(*sceneAsset.metadata, sceneAssetInfo)) {
    OBS_LOG_ERR("Failed to read scene asset info.");
    return;
  }

  std::string sceneJsonStr;
  sceneJsonStr.resize(sceneAssetInfo.unpackedSize);

  if (!asset::unpackAsset(sceneAssetInfo, sceneAsset.binaryBlob.data(),
                          sceneAsset.binaryBlob.size(), sceneJsonStr.data())) {
    OBS_LOG_ERR("Failed to unpack scene asset.");
    return;
  }

  nlohmann::json sceneJson;
  try {
    sceneJson = nlohmann::json::parse(sceneJsonStr);
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return;
  }

  ObsidianEngineContext& ctx = engine.getContext();
  serialization::SceneData sceneData;
  if (serialization::deserializeScene(sceneJson, sceneData)) {
    scene.resetState();
    scene.loadFromData(sceneData);
  } else {
    OBS_LOG_ERR("Failed to deserialize scene");
    return;
  }

  selectedGameObj = nullptr;
}

void saveScene(char const scenePath[], scene::Scene& scene) {
  nlohmann::json sceneJson;

  if (!serialization::serializeScene(scene.getData(), sceneJson)) {
    OBS_LOG_ERR("Failed to serialize the scene.");
    return;
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
    return;
  }

  if (!asset::saveToFile(project.getAbsolutePath(scenePath), sceneAsset)) {
    OBS_LOG_ERR("Failed to save scene.");
  }
}

void selectGameObjMesh(fs::path const& meshRelativePath) {
  selectedGameObjMeshAssetInfo = {};

  if (meshRelativePath.empty()) {
    selectedGameObjMats.clear();
    return;
  }

  asset::Asset meshAsset;
  asset::loadAssetFromFile(project.getAbsolutePath(meshRelativePath),
                           meshAsset);
  asset::readMeshAssetInfo(*meshAsset.metadata, selectedGameObjMeshAssetInfo);

  selectedGameObjMats.clear();
  selectedGameObjMats.resize(
      selectedGameObjMeshAssetInfo.indexBufferSizes.size(), -1);

  constexpr bool materialsIncludeNone = false;

  for (std::size_t i = 0; i < selectedGameObjMats.size(); ++i) {
    if (selectedGameObj->getMaterialRelativePaths().size() > i) {
      fs::path const materialRelativePath =
          selectedGameObj->getMaterialRelativePaths()[i];

      selectedGameObjMats[i] = materialsInProj.listItemInd(
          materialRelativePath, materialsIncludeNone);
    }

    if (selectedGameObjMats[i] < 0 &&
        selectedGameObjMeshAssetInfo.defaultMatRelativePaths.size()) {
      selectedGameObjMats[i] = materialsInProj.listItemInd(
          selectedGameObjMeshAssetInfo.defaultMatRelativePaths[i],
          materialsIncludeNone);
    }
  }
}

void selectGameObject(scene::GameObject& gameObject) {
  selectedGameObj = &gameObject;

  fs::path const& meshRelativePath = selectedGameObj->getMeshRelativePath();

  meshComboIndex = meshesInProj.listItemInd(meshRelativePath, meshIncludeNone);
  selectGameObjMesh(meshRelativePath);
}

void performImport(ObsidianEngine& engine, fs::path const& srcPath,
                   fs::path const& dstPath, core::MaterialType matType) {
  if (engine.isInitialized()) {
    engine.getContext().taskExecutor.enqueue(
        task::TaskType::general, [&engine, srcPath, dstPath, matType]() {
          obsidian::asset_converter::AssetConverter converter{
              engine.getContext().taskExecutor};
          converter.setMaterialType(matType);
          converter.convertAsset(srcPath, dstPath);
          assetListDirty = true;
        });
  } else {
    static obsidian::task::TaskExecutor executor;
    static bool executorInitialized = false;

    if (!executorInitialized) {
      executor.initAndRun({{obsidian::task::TaskType::general,
                            std::thread::hardware_concurrency()}});
      executorInitialized = true;
    }

    executor.enqueue(
        task::TaskType::general, [srcPath = srcPath, dstPath, matType]() {
          obsidian::asset_converter::AssetConverter converter{executor};
          converter.setMaterialType(matType);
          converter.convertAsset(srcPath, dstPath);
          assetListDirty = true;
        });
  }
}

void importPopup(ObsidianEngine& engine) {
  static fs::path importPath;
  constexpr char const* popupName = "Import file";

  if (!ImGui::IsPopupOpen(popupName) && _pendingDraggedFiles.size()) {
    importPath = _pendingDraggedFiles.back();
    _pendingDraggedFiles.pop_back();
    ImGui::OpenPopup(popupName);
  }

  if (ImGui::BeginPopup(popupName)) {
    ImGui::Text("Import %s", importPath.c_str());

    fs::path dstPath = project.getAbsolutePath(importPath.filename());
    static core::MaterialType matType = core::MaterialType::lit;

    if (dstPath.extension() == ".gltf" || dstPath.extension() == ".glb" ||
        dstPath.extension() == ".obj") {
      int matInd = static_cast<int>(matType);
      if (ImGui::Combo("Render Pipeline", &matInd, materialTypes.data(),
                       materialTypes.size())) {
        matType = static_cast<core::MaterialType>(matInd);
      }
    }

    if (ImGui::Button("Import")) {
      dstPath.replace_extension("");

      performImport(engine, importPath, dstPath, matType);

      ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void gameObjectHierarchy(scene::GameObject& gameObject) {
  bool selected = &gameObject == selectedGameObj;

  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  }

  std::string_view gameObjName = gameObject.getName();
  if (ImGui::TreeNode(gameObjName.data())) {
    if (ImGui::IsItemClicked()) {
      selectGameObject(gameObject);
    }

    if (selected) {
      ImGui::PopStyleColor();
    }

    ImGui::PushID(gameObject.getId());
    if (ImGui::Button("+")) {
      gameObject.createChild();
    }

    ImGui::SameLine();

    if (ImGui::Button("-")) {
      pendingObjDelete = &gameObject;
    }

    ImGui::PopID();

    for (scene::GameObject& childObject : gameObject.getChildren()) {
      gameObjectHierarchy(childObject);
    }

    ImGui::TreePop();
  } else {
    if (ImGui::IsItemClicked()) {
      selectGameObject(gameObject);
    }

    if (selected) {
      ImGui::PopStyleColor();
    }
  }
}

void engineTab(SceneData& sceneData, ObsidianEngine& engine) {
  ZoneScoped;

  ImGuiTabItemFlags engineTabFlags = 0;
  if (openEngineTab) {
    engineTabFlags = ImGuiTabItemFlags_SetSelected;
    openEngineTab = false;
  }

  if (ImGui::BeginTabItem("Engine", NULL, engineTabFlags)) {
    if (engine.isInitialized()) {
      if (ImGui::CollapsingHeader("Global Scene Params")) {
        ImGui::SliderFloat("Ambient light color r", &sceneData.ambientColor.r,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color g", &sceneData.ambientColor.g,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color b", &sceneData.ambientColor.b,
                           0.f, 1.f);
      }

      scene::Scene& scene = engine.getContext().scene;

      if (ImGui::CollapsingHeader("Scene")) {
        static char scenePath[maxPathSize] = "scene.obsscene";
        ImGui::InputText("Save file name", scenePath, maxPathSize);

        ImGui::NewLine();

        bool disabled = !std::strlen(scenePath);

        if (disabled) {
          ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                              ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Save Scene")) {
          if (std::filesystem::exists(project.getAbsolutePath(scenePath))) {
            ImGui::OpenPopup("File already exists");
          } else {
            saveScene(scenePath, scene);
          }
        }

        if (ImGui::BeginPopup("File already exists")) {
          ImGui::Text("File with the same name already exists. Are you sure "
                      "you want to overwrite the existing file?");
          ImGui::BeginGroup();

          if (ImGui::Button("Overwrite")) {
            saveScene(scenePath, scene);
            ImGui::CloseCurrentPopup();
          }

          ImGui::SameLine();

          if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
          }

          ImGui::EndGroup();
          ImGui::EndPopup();
        }

        if (ImGui::Button("Load Scene")) {
          loadScene(scenePath, scene, engine);
        }

        if (disabled) {
          ImGui::PopStyleVar();
          ImGui::PopItemFlag();
        }

        ImGui::NewLine();
        ImGui::SeparatorText("Object Hierarchy");

        if (ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::Button("+")) {
            scene.createGameObject();
          }
          for (auto& gameObject : scene.getGameObjects()) {
            gameObjectHierarchy(gameObject);
          }
          ImGui::TreePop();
        }

        if (pendingObjDelete) {
          scene::GameObject* const parent = pendingObjDelete->getParent();

          selectedGameObj = parent;

          if (parent) {
            selectedGameObj = parent;
            parent->destroyChild(pendingObjDelete->getId());
          } else {
            scene.destroyGameObject(pendingObjDelete->getId());
          }

          pendingObjDelete = nullptr;
        }
      }

      if (selectedGameObj) {
        ImGui::NewLine();
        ImGui::SeparatorText("Game Object");

        char gameObjectName[maxGameObjectNameSize];
        std::string_view const selectedGameObjName = selectedGameObj->getName();
        std::strncpy(gameObjectName, selectedGameObjName.data(),
                     selectedGameObjName.size() + 1);

        ImGui::InputText("Name", gameObjectName, std::size(gameObjectName));

        selectedGameObj->setName(gameObjectName);

        glm::vec3 pos = selectedGameObj->getPosition();
        ImGui::InputScalarN("Position", ImGuiDataType_Float, &pos, 3);
        selectedGameObj->setPosition(pos);

        // we need to track separate buffer for euler angles because glm
        // conversions do normalization on euler angles to keep the yaw in range
        // -90 and 90 degrees
        static scene::GameObject const* lastSelected = nullptr;
        static glm::vec3 gameObjEuler;

        if (selectedGameObj != lastSelected) {
          lastSelected = selectedGameObj;
          gameObjEuler = selectedGameObj->getEuler();
        }

        if (ImGui::SliderFloat3("Euler Rotation",
                                reinterpret_cast<float*>(&gameObjEuler),
                                -180.0f, 180.f)) {
          selectedGameObj->setEuler(gameObjEuler);
        }

        glm::vec3 scale = selectedGameObj->getScale();
        ImGui::InputScalarN("Scale", ImGuiDataType_Float, &scale, 3);
        selectedGameObj->setScale(scale);

        ImGui::NewLine();
        ImGui::SeparatorText("Mesh and Materials");

        ValueStrings const meshSizeAndStrings =
            meshesInProj.getValueStrings(meshIncludeNone);

        if (ImGui::Combo("Mesh", &meshComboIndex,
                         meshSizeAndStrings.valueStrings,
                         meshSizeAndStrings.size)) {
          selectGameObjMesh(meshesInProj.at(meshComboIndex, meshIncludeNone));
        }

        selectedGameObjMats.resize(
            selectedGameObjMeshAssetInfo.indexBufferSizes.size(), 0);

        constexpr bool materialsIncludeNone = false;
        ValueStrings const materialValueStrings =
            materialsInProj.getValueStrings(materialsIncludeNone);

        ImGui::NewLine();
        if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (std::size_t i = 0; i < selectedGameObjMats.size(); ++i) {
            if (ImGui::Combo(std::to_string(i).c_str(), &selectedGameObjMats[i],
                             materialValueStrings.valueStrings,
                             materialValueStrings.size)) {
            }
          }

          ImGui::TreePop();
        }

        ImGui::NewLine();
        bool disabled =
            !materialsInProj.valuesSize() || !meshesInProj.valuesSize();

        if (disabled) {
          ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                              ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Apply Mesh and Material")) {
          fs::path const selctedMeshPath =
              meshesInProj.at(meshComboIndex, meshIncludeNone);
          selectedGameObj->setMesh(selctedMeshPath);

          std::vector<fs::path> selectedMaterialPaths;
          for (int selectedMaterial : selectedGameObjMats) {
            selectedMaterialPaths.push_back(
                materialsInProj.at(selectedMaterial, materialsIncludeNone));
          }

          selectedGameObj->setMaterials(selectedMaterialPaths);
        }

        if (disabled) {
          ImGui::PopItemFlag();
          ImGui::PopStyleVar();
        }

        ImGui::NewLine();
        ImGui::SeparatorText("Lights");

        std::optional<core::DirectionalLight> directionalLight =
            selectedGameObj->getDirectionalLight();

        if (!directionalLight) {
          if (ImGui::Button("Add directional light")) {
            selectedGameObj->setDirectionalLight({});
          }
        } else {
          ImGui::PushID("DirectionalLight");

          bool dirLightUpdated = false;

          dirLightUpdated |= ImGui::SliderFloat3(
              "Color", reinterpret_cast<float*>(&directionalLight->color), 0.0f,
              1.0f);

          dirLightUpdated |= ImGui::SliderFloat(
              "Intensity", &directionalLight->intensity, 0.0f, 200.0f);

          if (dirLightUpdated) {
            selectedGameObj->setDirectionalLight(*directionalLight);
          }

          if (ImGui::Button("Remove")) {
            selectedGameObj->removeDirectionalLight();
          }

          ImGui::PopID();
        }

        std::optional<core::Spotlight> spotlight =
            selectedGameObj->getSpotlight();

        if (!spotlight) {
          if (ImGui::Button("Add spotlight")) {
            selectedGameObj->setSpotlight({});
          }
        } else {
          ImGui::PushID("Spotlight");

          bool spotlightUpdated = false;

          spotlightUpdated |= ImGui::SliderFloat3(
              "Color", reinterpret_cast<float*>(&spotlight->color), 0.0f, 1.0f);

          spotlightUpdated |= ImGui::SliderFloat(
              "Intensity", &spotlight->intensity, 0.0f, 200.0f);

          spotlightUpdated |= ImGui::SliderFloat(
              "Cutoff angle", &spotlight->cutoffAngleRad, 0.01f, 3.14f);

          spotlightUpdated |=
              ImGui::SliderFloat("Fadeout angle", &spotlight->fadeoutAngleRad,
                                 spotlight->cutoffAngleRad, 3.14f);

          spotlightUpdated |= ImGui::SliderFloat(
              "Linear attenuation", &spotlight->linearAttenuation, 0.0f, 1.0f);

          spotlightUpdated |=
              ImGui::SliderFloat("Quadratic attenuation",
                                 &spotlight->quadraticAttenuation, 0.0f, 1.0f);

          if (spotlightUpdated) {
            selectedGameObj->setSpotlight(*spotlight);
          }

          if (ImGui::Button("Remove")) {
            selectedGameObj->removeSpotlight();
          }

          ImGui::PopID();
        }

        ImGui::NewLine();
        ImGui::SeparatorText("Environment Map");

        if (!selectedGameObj->hasEnvironmentMap()) {
          if (ImGui::Button("Add")) {
            selectedGameObj->setEnvironmentMap();
          }
        } else {
          float envMapRadius = selectedGameObj->getEnvironmentMapRadius();

          if (ImGui::SliderFloat("Radius", &envMapRadius, 0.0f, 50.0f)) {
            selectedGameObj->setEnvironmentMap(envMapRadius);
          }

          if (ImGui::Button("Remove")) {
            selectedGameObj->removeEnvironmentMap();
          }
        }
      }
    } else {
      if (ImGui::Button("Start Engine")) {
        engine.init(sdl_wrapper::SDLBackend::instance(),
                    project.getOpenProjectPath());
        openEngineTab = true;
      }
    }

    ImGui::EndTabItem();
  }
}

void importTab(ObsidianEngine& engine) {
  ZoneScoped;

  if (ImGui::BeginTabItem("Import")) {
    static char srcFilePath[maxPathSize];
    ImGui::InputText("Src file path", srcFilePath, std::size(srcFilePath));

    static char dstFileName[maxPathSize];
    ImGui::InputText("Dst file name", dstFileName, std::size(dstFileName));

    if (ImGui::Button("Convert")) {
      fs::path destPath = project.getAbsolutePath(dstFileName);
      obsidian::asset_converter::AssetConverter converter{
          engine.getContext().taskExecutor};
      converter.convertAsset(srcFilePath, destPath);
    }

    ImGui::EndTabItem();
  }
}

constexpr bool shadersIncludeNone = false;

static struct {
  int selectedMaterialInd = -1;
  bool materialSelectionUpdated = false;
  int selectedMaterialType = static_cast<int>(core::MaterialType::lit);
  int vertexShaderComboInd = 0;
  int fragmentShaderComboInd = 0;
  asset::MaterialAssetInfo selectedMaterialAssetInfo;
  char newMatName[maxFileNameSize] = "newMat";

} materialsData;

void unlitMaterialEditor(asset::UnlitMaterialAssetData& unlitMatData) {
  constexpr bool texIncludeNone = true;
  int colorTexComboInd =
      texturesInProj.listItemInd(unlitMatData.colorTexturePath, texIncludeNone);

  auto const texValueStrings = texturesInProj.getValueStrings(texIncludeNone);

  if (ImGui::Combo("Color Tex", &colorTexComboInd, texValueStrings.valueStrings,
                   texValueStrings.size)) {
    unlitMatData.colorTexturePath =
        texturesInProj.at(colorTexComboInd, texIncludeNone).string();
  }

  if (ImGui::SliderFloat4("Color", &unlitMatData.color.r, 0.0f, 1.0f)) {
  }
}

void litMaterialEditor(asset::LitMaterialAssetData& litMatData) {
  constexpr bool texturesIncludeNone = true;

  int diffuseTexComboInd = texturesInProj.listItemInd(
      litMatData.diffuseTexturePath, texturesIncludeNone);

  ValueStrings const texValueStrings =
      texturesInProj.getValueStrings(texturesIncludeNone);

  if (ImGui::Combo("Diffuse Tex", &diffuseTexComboInd,
                   texValueStrings.valueStrings, texValueStrings.size)) {
    litMatData.diffuseTexturePath =
        texturesInProj.at(diffuseTexComboInd, texturesIncludeNone).string();
  }

  int normalTexComboInd = texturesInProj.listItemInd(
      litMatData.normalMapTexturePath, texturesIncludeNone);

  if (ImGui::Combo("Normal Tex", &normalTexComboInd,
                   texValueStrings.valueStrings, texValueStrings.size)) {
    litMatData.normalMapTexturePath =
        texturesInProj.at(normalTexComboInd, texturesIncludeNone).string();
  }

  if (ImGui::SliderFloat4("Ambient Color", &litMatData.ambientColor.r, 0.0f,
                          1.0f)) {
  }

  if (ImGui::SliderFloat4("Diffuse Color", &litMatData.diffuseColor.r, 0.0f,
                          1.0f)) {
  }

  if (ImGui::SliderFloat4("Specular Color", &litMatData.specularColor.r, 0.0f,
                          1.0f)) {
  }

  if (ImGui::SliderFloat("Shininess", &litMatData.shininess, 1.0f, 256.0f)) {
  }

  if (ImGui::Checkbox("Reflection", &litMatData.reflection)) {
  }
}

void pbrMaterialEditor(asset::PBRMaterialAssetData& pbrMatData) {}

void materialCreatorTab() {
  ZoneScoped;

  constexpr bool materialsIncludeNone = false;
  ValueStrings const materialValueStrings =
      materialsInProj.getValueStrings(materialsIncludeNone);

  if (ImGui::BeginTabItem("Materials")) {
    if (assetListDirty) {
      materialsData.selectedMaterialInd = materialsInProj.valuesSize() ? 0 : -1;
      materialsData.materialSelectionUpdated = true;
      ImGui::EndTabItem();
      return;
    }

    ImGui::InputText("New Material Name", materialsData.newMatName,
                     sizeof(materialsData.newMatName));

    int const newMatNameLen = std::strlen(materialsData.newMatName);

    fs::path newMaterialRelPath = materialsData.newMatName;
    newMaterialRelPath.replace_extension(globals::materialAssetExt);

    auto const matWithSameNameExists =
        [&newMaterialRelPath](fs::path const& matPath) {
          return newMaterialRelPath == matPath;
        };

    bool disabled =
        !newMatNameLen || materialsInProj.exists(newMaterialRelPath);

    if (disabled) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Create")) {
      asset::MaterialAssetInfo newMatAssetInfo = {};
      newMatAssetInfo.materialType = core::MaterialType::lit;
      newMatAssetInfo.vertexShaderPath =
          "obsidian/shaders/default-vert.obsshad";
      newMatAssetInfo.fragmentShaderPath =
          "obsidian/shaders/default-frag.obsshad";
      asset::LitMaterialAssetData& litMaterialData =
          newMatAssetInfo.materialSubtypeData
              .emplace<asset::LitMaterialAssetData>();
      litMaterialData.ambientColor = {1.0f, 1.0f, 1.0f, 1.0f};
      litMaterialData.diffuseColor = {1.0f, 1.0f, 1.0f, 1.0f};
      litMaterialData.specularColor = {1.0f, 1.0f, 1.0f, 1.0f};
      litMaterialData.shininess = 1.0f;

      asset::Asset newMatAsset;

      fs::path newMaterialAbsPath =
          project.getAbsolutePath(materialsData.newMatName);
      newMaterialAbsPath.replace_extension(globals::materialAssetExt);

      if (asset::packMaterial(newMatAssetInfo, {}, newMatAsset)) {
        if (asset::saveToFile(newMaterialAbsPath, newMatAsset)) {
          assetListDirty = true;
        } else {
          OBS_LOG_ERR("Failed to save asset to path " +
                      newMaterialAbsPath.string());
        }
      } else {
        OBS_LOG_ERR("Failed to pack material.");
      }
    }

    if (disabled) {
      ImGui::PopStyleVar();
      ImGui::PopItemFlag();
    }

    ImGui::NewLine();

    if (ImGui::BeginListBox("Materials")) {
      for (int i = 0; i < materialValueStrings.size; ++i) {
        bool selected = materialsData.selectedMaterialInd == i;

        if (ImGui::Selectable(materialValueStrings.valueStrings[i],
                              &selected)) {
          materialsData.selectedMaterialInd = i;
          materialsData.materialSelectionUpdated = true;
        }
      }

      ImGui::EndListBox();
    }

    ImGui::NewLine();

    if (materialsData.materialSelectionUpdated &&
        materialsData.selectedMaterialInd < materialsInProj.valuesSize()) {
      fs::path const absolutePath = project.getAbsolutePath(materialsInProj.at(
          materialsData.selectedMaterialInd, materialsIncludeNone));
      asset::Asset matAsset;
      if (asset::loadAssetFromFile(absolutePath, matAsset) ||
          !matAsset.metadata) {
        if (asset::readMaterialAssetInfo(
                *matAsset.metadata, materialsData.selectedMaterialAssetInfo)) {

        } else {
          OBS_LOG_ERR("Failed to load material asset info from asset on path " +
                      absolutePath.string());
        }
      } else {
        OBS_LOG_ERR("Failed to load asset from file on path " +
                    absolutePath.string());
      }

      materialsData.selectedMaterialType = static_cast<int>(
          materialsData.selectedMaterialAssetInfo.materialType);

      materialsData.vertexShaderComboInd = shadersInProj.listItemInd(
          materialsData.selectedMaterialAssetInfo.vertexShaderPath,
          shadersIncludeNone);

      materialsData.fragmentShaderComboInd = shadersInProj.listItemInd(
          materialsData.selectedMaterialAssetInfo.fragmentShaderPath,
          shadersIncludeNone);

      materialsData.materialSelectionUpdated = false;
    }

    if (materialsData.selectedMaterialInd >= 0) {
      ImGui::SeparatorText(
          materialValueStrings.valueStrings[materialsData.selectedMaterialInd]);

      if (ImGui::Combo("Material Type", &materialsData.selectedMaterialType,
                       materialTypes.data(), materialTypes.size())) {
        materialsData.selectedMaterialAssetInfo.materialType =
            static_cast<core::MaterialType>(materialsData.selectedMaterialType);
        materialsData.selectedMaterialAssetInfo.materialSubtypeData =
            asset::createSubtypeData(
                materialsData.selectedMaterialAssetInfo.materialType);
      }

      ValueStrings const shaderSizeAndStrings =
          shadersInProj.getValueStrings(shadersIncludeNone);

      if (ImGui::Combo("Vertex Shader", &materialsData.vertexShaderComboInd,
                       shaderSizeAndStrings.valueStrings,
                       shaderSizeAndStrings.size)) {
        materialsData.selectedMaterialAssetInfo.vertexShaderPath =
            shadersInProj
                .at(materialsData.vertexShaderComboInd, shadersIncludeNone)
                .string();
      }

      if (ImGui::Combo("Fragment Shader", &materialsData.vertexShaderComboInd,
                       shaderSizeAndStrings.valueStrings,
                       shaderSizeAndStrings.size)) {
        materialsData.selectedMaterialAssetInfo.fragmentShaderPath =
            shadersInProj
                .at(materialsData.fragmentShaderComboInd, shadersIncludeNone)
                .string();
      }

      if (ImGui::Checkbox(
              "Transparent",
              &materialsData.selectedMaterialAssetInfo.transparent)) {
      }

      if (ImGui::Checkbox("Uses Timer",
                          &materialsData.selectedMaterialAssetInfo.hasTimer)) {
      }

      std::visit(core::visitor{[](asset::UnlitMaterialAssetData& unlitData) {
                                 unlitMaterialEditor(unlitData);
                               },
                               [](asset::LitMaterialAssetData& litData) {
                                 litMaterialEditor(litData);
                               },
                               [](asset::PBRMaterialAssetData& pbrData) {}},
                 materialsData.selectedMaterialAssetInfo.materialSubtypeData);

      if (ImGui::Button("Update")) {
        asset::Asset materialAsset;
        asset::packMaterial(materialsData.selectedMaterialAssetInfo, {},
                            materialAsset);
        fs::path selectedMathAbsPath = project.getAbsolutePath(
            materialValueStrings
                .valueStrings[materialsData.selectedMaterialInd]);
        selectedMathAbsPath.replace_extension(".obsmat");
        asset::saveToFile(selectedMathAbsPath, materialAsset);
        assetListDirty = true;
      }
    }

    ImGui::EndTabItem();
  }
}

void textureEditorTab() {
  ZoneScoped;

  if (ImGui::BeginTabItem("Textures")) {
    static int textureComboInd = 0;
    static asset::TextureAssetInfo selectedTextureAssetInfo;
    static asset::Asset selectedTextureAsset;
    static bool isInitialized = false;

    auto const loadTextureData = [](fs::path const& texturePath) {
      selectedTextureAsset = {};
      asset::loadAssetFromFile(project.getAbsolutePath(texturePath),
                               selectedTextureAsset);
      selectedTextureAssetInfo = {};
      asset::readTextureAssetInfo(*selectedTextureAsset.metadata,
                                  selectedTextureAssetInfo);
    };

    constexpr bool texIncludeNone = false;
    ValueStrings const texValueStrings =
        texturesInProj.getValueStrings(texIncludeNone);

    if (texValueStrings.size) {
      if (!isInitialized) {
        loadTextureData(texturesInProj.at(textureComboInd, texIncludeNone));
        isInitialized = true;
      }

      if (ImGui::Combo("Texture", &textureComboInd,
                       texValueStrings.valueStrings, texValueStrings.size) &&
          textureComboInd) {
        loadTextureData(texturesInProj.at(textureComboInd, texIncludeNone));
      }

      int currentSelectedFormat =
          static_cast<int>(selectedTextureAssetInfo.format);
      if (ImGui::Combo("Format", &currentSelectedFormat, textureTypes.data(),
                       (int)textureTypes.size())) {
        selectedTextureAssetInfo.format =
            static_cast<core::TextureFormat>(currentSelectedFormat);
      }

      if (ImGui::Button("Save")) {
        asset::updateTextureAssetInfo(selectedTextureAssetInfo,
                                      selectedTextureAsset);

        asset::saveToFile(project.getAbsolutePath(texturesInProj.at(
                              textureComboInd, texIncludeNone)),
                          selectedTextureAsset);
      }
    }

    ImGui::EndTabItem();
  }
}

void projectTab(ObsidianEngine& engine) {
  ZoneScoped;

  if (ImGui::BeginTabItem("Project")) {
    static char projPathBuf[maxPathSize];
    ImGui::InputText("Project Path", projPathBuf, std::size(projPathBuf));

    if (ImGui::Button("Browse...")) {
      ImGuiFileDialog::Instance()->OpenDialog(
          "OpenProjectDirectoryDlg", "Open project directory", nullptr);
    }

    if (ImGuiFileDialog::Instance()->Display("OpenProjectDirectoryDlg",
                                             ImGuiWindowFlags_NoCollapse |
                                                 ImGuiWindowFlags_NoDocking,
                                             ImVec2(300, 300))) {
      if (ImGuiFileDialog::Instance()->IsOk()) {
        std::string const projectPath =
            ImGuiFileDialog::Instance()->GetCurrentPath();
        strncpy(projPathBuf, projectPath.c_str(), projectPath.size());
      }

      ImGuiFileDialog::Instance()->Close();
    }

    std::size_t projPathLen = std::strlen(projPathBuf);
    bool disabled = projPathLen == 0;

    if (disabled) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Open")) {
      selectedGameObj = nullptr;
      if (engine.isInitialized()) {
        engine.openProject(projPathBuf);
      }
      project.open(projPathBuf);
      setLastOpenProject(projPathBuf);
      assetListDirty = true;
    }

    if (disabled) {
      ImGui::PopItemFlag();
      ImGui::PopStyleVar();
    }

    if (ImGui::Button("Open sample scene")) {
      fs::path sampleProjectPath = core::utils::findDirInParentTree(
          obsidian::platform::getExecutableDirectoryPath(),
          SAMPLE_PROJECT_DIR_NAME);

      if (sampleProjectPath.empty()) {
        OBS_LOG_ERR("Cannot find sample project path.");
      } else {
        project.open(sampleProjectPath);
        setLastOpenProject(sampleProjectPath);
        assetListDirty = true;
        openEngineTab = engine.init(sdl_wrapper::SDLBackend::instance(),
                                    project.getOpenProjectPath());
        loadScene("scene.obsscene", engine.getContext().scene, engine);
      }
    }

    if (project.getOpenProjectPath().empty()) {
      fs::path lastOpenProject = getLastOpenProject();

      if (!lastOpenProject.empty()) {
        if (ImGui::Button("Load last project")) {
          project.open(lastOpenProject);
          std::string const lastOpenProjectStr = lastOpenProject.string();
          std::strncpy(projPathBuf, lastOpenProjectStr.c_str(),
                       lastOpenProjectStr.size());
          assetListDirty = true;
        }

        if (ImGui::Button("Load last project and run")) {
          if (project.open(lastOpenProject)) {
            std::string const lastOpenProjectStr = lastOpenProject.string();
            std::strncpy(projPathBuf, lastOpenProjectStr.c_str(),
                         lastOpenProjectStr.size());
            assetListDirty = true;
            openEngineTab = engine.init(sdl_wrapper::SDLBackend::instance(),
                                        project.getOpenProjectPath());
          }
        }
      }
    } else {
      if (ImGui::BeginTabBar("EditorTabBar")) {
        materialCreatorTab();
        textureEditorTab();
        importTab(engine);
        ImGui::EndTabBar();
      }
    }

    ImGui::EndTabItem();
  }
}

void begnEditorFrame(ImGuiIO& imguiIO) {
  ZoneScoped;

  if (assetListDirty) {
    refreshAssetLists();
    assetListDirty = false;
  }

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(imguiIO.DisplaySize);
  ImGui::SetNextWindowPos({0, 0});
}

void endEditorFrame(SDL_Renderer& renderer, ImGuiIO& imguiIO) {
  ZoneScoped;

  // Rendering
  ImGui::Render();
  ImGui::UpdatePlatformWindows();
  ImGui::RenderPlatformWindowsDefault();
  SDL_RenderSetScale(&renderer, imguiIO.DisplayFramebufferScale.x,
                     imguiIO.DisplayFramebufferScale.y);

  ImVec4 const clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  SDL_SetRenderDrawColor(
      &renderer, (Uint8)(clearColor.x * 255), (Uint8)(clearColor.y * 255),
      (Uint8)(clearColor.z * 255), (Uint8)(clearColor.w * 255));
  SDL_RenderClear(&renderer);

  {
    ZoneScopedN("ImGui_ImplSDLRenderer2_RenderDrawData");
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
  }

  {
    ZoneScopedN("SDL_RenderPresent");
    SDL_RenderPresent(&renderer);
  }
}

void editorWindow(SDL_Renderer& renderer, ImGuiIO& imguiIO,
                  DataContext& context, ObsidianEngine& engine) {
  ZoneScoped;

  ImGui::Begin("EditorWindow", NULL, ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (ImGui::BeginTabBar("EditorTabBar")) {

    projectTab(engine);
    if (!project.getOpenProjectPath().empty()) {
      engineTab(context.sceneData, engine);
    }

    ImGui::EndTabBar();
  }

  importPopup(engine);

  ImGui::End();
}

void instantiatePrefab(fs::path const& prefabPath, ObsidianEngine& engine) {
  asset::Asset prefabAsset;

  if (!asset::loadAssetFromFile(prefabPath, prefabAsset)) {
    OBS_LOG_ERR("Failed to load prefab on path " + prefabPath.string());
    return;
  } else if (!prefabAsset.metadata) {
    OBS_LOG_ERR("Prefab asset metadata missing from loaded asset.");
    return;
  }

  asset::PrefabAssetInfo prefabAssetInfo;

  if (!asset::readPrefabAssetInfo(*prefabAsset.metadata, prefabAssetInfo)) {
    OBS_LOG_ERR("Failed to read prefab asset info.");
    return;
  }

  std::string prefabJsonStr;
  prefabJsonStr.resize(prefabAssetInfo.unpackedSize);

  if (!asset::unpackAsset(prefabAssetInfo, prefabAsset.binaryBlob.data(),
                          prefabAsset.binaryBlob.size(),
                          prefabJsonStr.data())) {
    OBS_LOG_ERR("Failed to unpack asset.");
    return;
  }

  nlohmann::json prefabJson;

  try {
    prefabJson = nlohmann::json::parse(prefabJsonStr);
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return;
  }

  serialization::GameObjectData gameObjectData;
  if (!serialization::deserializeGameObject(prefabJson, gameObjectData)) {
    OBS_LOG_ERR("Failed to deserialize game object data.");
    return;
  }

  ObsidianEngineContext& ctx = engine.getContext();
  scene::GameObject& obj = ctx.scene.createGameObject();
  obj.populate(gameObjectData);
}

void fileDropped(const char* file, ObsidianEngine& engine) {
  fs::path const srcPath{file};

  if (srcPath.extension() == globals::prefabAssetExt) {
    // prefab instantiation
    if (!engine.isInitialized()) {
      OBS_LOG_ERR("Can't instantiate prefab if the engine is not initialized.");
      return;
    }

    instantiatePrefab(srcPath, engine);
  } else {
    // asset import

    if (project.getOpenProjectPath().empty()) {
      OBS_LOG_ERR("File dropped in but no project was open.");
      return;
    }

    _pendingDraggedFiles.push_back(srcPath);
  }
}

} /*namespace obsidian::editor*/
