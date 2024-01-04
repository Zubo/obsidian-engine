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
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/editor/settings.hpp>
#include <obsidian/globals/file_extensions.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/scene/serialization.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <glm/ext/vector_int3.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

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
static int meshDropdownSelectedIndex = std::numeric_limits<int>::min();
static asset::MeshAssetInfo selectedGameObjMeshAssetInfo;
static std::vector<fs::path> texturesInProj;
static std::vector<fs::path> shadersInProj;
static std::vector<fs::path> materialsInProj;
static std::vector<fs::path> meshesInProj;
static std::vector<char const*> texturePathStringPtrs;
static std::vector<char const*> shaderPathStringPtrs;
static std::vector<char const*> materialPathStringPtrs;
static std::vector<char const*> meshesPathStringPtrs;
static bool assetListDirty = false;
static std::array<char const*, 2> materialTypes = {"unlit", "lit"};
static std::array<char const*, 4> textureTypes = {
    "Unknown", "R8G8B8A8_SRGB", "R8G8B8A8_LINEAR", "R32G32_SFLOAT"};
static bool openEngineTab = false;
scene::GameObject* pendingObjDelete = nullptr;

void refreshAssetLists() {
  texturesInProj = project.getAllFilesWithExtension(globals::textureAssetExt);
  texturePathStringPtrs.clear();

  texturePathStringPtrs.push_back("none");

  auto transformToStr = [](auto& dst, auto& src) {
    std::transform(src.cbegin(), src.cend(), std::back_inserter(dst),
                   [](auto const& arg) { return arg.c_str(); });
  };

  transformToStr(texturePathStringPtrs, texturesInProj);

  shaderPathStringPtrs.clear();
  shadersInProj = project.getAllFilesWithExtension(globals::shaderAssetExt);

  transformToStr(shaderPathStringPtrs, shadersInProj);

  materialPathStringPtrs.clear();
  materialsInProj = project.getAllFilesWithExtension(globals::materialAssetExt);

  transformToStr(materialPathStringPtrs, materialsInProj);

  meshesInProj = project.getAllFilesWithExtension(globals::meshAssetExt);

  meshesPathStringPtrs.clear();
  meshesPathStringPtrs.push_back("none");
  transformToStr(meshesPathStringPtrs, meshesInProj);
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

void loadScene(char const scenePath[], scene::SceneState& sceneState,
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

  sceneState = {};

  ObsidianEngineContext& ctx = engine.getContext();
  if (!scene::deserializeScene(sceneJson, ctx.vulkanRHI, ctx.resourceManager,
                               sceneState)) {
    OBS_LOG_ERR("Failed to deserialize scene");
    return;
  }

  selectedGameObj = nullptr;
}

void saveScene(char const scenePath[], scene::SceneState& sceneState) {
  nlohmann::json sceneJson;

  if (!scene::serializeScene(sceneState, sceneJson)) {
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

void selectGameObjMesh(int gameObjMeshInd) {
  if (gameObjMeshInd + 1 == meshDropdownSelectedIndex) {
    return;
  }

  meshDropdownSelectedIndex = gameObjMeshInd + 1;
  selectedGameObjMeshAssetInfo = {};
  if (gameObjMeshInd < 0) {
    selectedGameObjMats.clear();
    return;
  }

  fs::path const& meshRelativePath = meshesInProj[gameObjMeshInd];

  asset::Asset meshAsset;
  asset::loadAssetFromFile(project.getAbsolutePath(meshRelativePath),
                           meshAsset);
  asset::readMeshAssetInfo(*meshAsset.metadata, selectedGameObjMeshAssetInfo);

  selectedGameObjMats.clear();
  selectedGameObjMats.resize(
      selectedGameObjMeshAssetInfo.indexBufferSizes.size(), -1);

  for (std::size_t i = 0; i < selectedGameObjMats.size(); ++i) {
    if (selectedGameObj->getMaterialRelativePaths().size() > i) {
      fs::path const materialRelativePath =
          selectedGameObj->getMaterialRelativePaths()[i];

      selectedGameObjMats[i] =
          indexorDefault(materialsInProj, materialRelativePath, -1);
    }

    if (selectedGameObjMats[i] < 0 &&
        selectedGameObjMeshAssetInfo.defaultMatRelativePaths.size()) {
      selectedGameObjMats[i] = indexorDefault(
          materialsInProj,
          selectedGameObjMeshAssetInfo.defaultMatRelativePaths[i], -1);
    }
  }
}

void selectGameObject(scene::GameObject& gameObject) {
  selectedGameObj = &gameObject;

  fs::path const& meshRelativePath = selectedGameObj->getMeshRelativePath();
  if (!meshRelativePath.empty()) {
    selectGameObjMesh(indexorDefault(meshesInProj, meshRelativePath, -1));
  } else {
    selectGameObjMesh(-1);
  }
}

void gameObjectHierarchy(scene::GameObject& gameObject,
                         scene::SceneState& sceneState) {
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
      gameObjectHierarchy(childObject, sceneState);
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

void engineTab(SceneData& sceneData, ObsidianEngine& engine,
               bool& engineStarted) {

  ImGuiTabItemFlags engineTabFlags = 0;
  if (openEngineTab) {
    engineTabFlags = ImGuiTabItemFlags_SetSelected;
    openEngineTab = false;
  }

  if (ImGui::BeginTabItem("Engine", NULL, engineTabFlags)) {
    if (engineStarted) {
      if (ImGui::CollapsingHeader("Global Scene Params")) {
        ImGui::SliderFloat("Ambient light color r", &sceneData.ambientColor.r,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color g", &sceneData.ambientColor.g,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color b", &sceneData.ambientColor.b,
                           0.f, 1.f);
      }

      scene::SceneState& sceneState = engine.getContext().scene.getState();

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
            saveScene(scenePath, sceneState);
          }
        }

        if (ImGui::BeginPopup("File already exists")) {
          ImGui::Text("File with the same name already exists. Are you sure "
                      "you want to overwrite the existing file?");
          ImGui::BeginGroup();

          if (ImGui::Button("Overwrite")) {
            saveScene(scenePath, sceneState);
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
          loadScene(scenePath, sceneState, engine);
        }

        if (disabled) {
          ImGui::PopStyleVar();
          ImGui::PopItemFlag();
        }

        ImGui::NewLine();
        ImGui::SeparatorText("Object Hierarchy");

        if (ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::Button("+")) {
            sceneState.gameObjects.emplace_back(
                engine.getContext().vulkanRHI,
                engine.getContext().resourceManager);
          }
          for (auto& gameObject : sceneState.gameObjects) {
            gameObjectHierarchy(gameObject, sceneState);
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
            auto const gameObjectIter = std::find_if(
                sceneState.gameObjects.cbegin(), sceneState.gameObjects.cend(),
                [d = pendingObjDelete](auto const& g) {
                  return d->getId() == g.getId();
                });

            if (gameObjectIter != sceneState.gameObjects.cend()) {
              sceneState.gameObjects.erase(gameObjectIter);
            }
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

        glm::vec3 euler = selectedGameObj->getEuler();
        ImGui::SliderFloat3("Euler Rotation", reinterpret_cast<float*>(&euler),
                            -180.0f, 180.f);
        selectedGameObj->setEuler(euler);

        glm::vec3 scale = selectedGameObj->getScale();
        ImGui::InputScalarN("Scale", ImGuiDataType_Float, &scale, 3);
        selectedGameObj->setScale(scale);

        ImGui::NewLine();
        ImGui::SeparatorText("Mesh and Materials");

        int selectedMeshInd = meshDropdownSelectedIndex;
        if (ImGui::Combo("Mesh", &selectedMeshInd, meshesPathStringPtrs.data(),
                         meshesPathStringPtrs.size())) {
          selectGameObjMesh(selectedMeshInd - 1);
        }

        selectedGameObjMats.resize(
            selectedGameObjMeshAssetInfo.indexBufferSizes.size(), 0);

        ImGui::NewLine();
        if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (std::size_t i = 0; i < selectedGameObjMats.size(); ++i) {
            if (ImGui::Combo(std::to_string(i).c_str(), &selectedGameObjMats[i],
                             materialPathStringPtrs.data(),
                             materialPathStringPtrs.size())) {
            }
          }

          ImGui::TreePop();
        }

        ImGui::NewLine();
        bool disabled =
            materialPathStringPtrs.empty() || meshesPathStringPtrs.empty();

        if (disabled) {
          ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                              ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Apply Mesh and Material")) {
          fs::path const selctedMeshPath =
              meshDropdownSelectedIndex
                  ? meshesInProj[meshDropdownSelectedIndex - 1]
                  : fs::path{};
          selectedGameObj->setMesh(selctedMeshPath);

          std::vector<fs::path> selectedMaterialPaths;
          for (int selectedMaterial : selectedGameObjMats) {
            selectedMaterialPaths.push_back(materialsInProj[selectedMaterial]);
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
              "Intensity", &directionalLight->intensity, 0.0f, 10.0f);

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
              "Intensity", &spotlight->intensity, 0.0f, 10.0f);

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
        engineStarted = engine.init(sdl_wrapper::SDLBackend::instance(),
                                    project.getOpenProjectPath());
        openEngineTab = true;
      }
    }

    ImGui::EndTabItem();
  }
}

void importTab(ObsidianEngine& engine) {
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

void materialCreatorTab() {

  if (ImGui::BeginTabItem("Materials")) {
    static int selectedMaterialInd = -1;
    static bool materialSelectionUpdated = false;

    if (assetListDirty) {
      selectedMaterialInd = -1;
      materialSelectionUpdated = true;
      ImGui::EndTabItem();
      return;
    }

    static int selectedMaterialType = static_cast<int>(core::MaterialType::lit);
    static int selectedDiffuseTex = 0;
    static int selectedNormalMapTex = 0;
    static int selectedShader = 0;
    static asset::MaterialAssetInfo selectedMaterialAssetInfo;
    static char newMatName[maxFileNameSize] = "newMat";

    ImGui::InputText("New Material Name", newMatName, sizeof(newMatName));

    int const newMatNameLen = std::strlen(newMatName);

    fs::path newMaterialRelPath = newMatName;
    newMaterialRelPath.replace_extension(globals::materialAssetExt);

    auto const matWithSameNameExists =
        [&newMaterialRelPath](fs::path const& matPath) {
          return newMaterialRelPath == matPath;
        };

    bool disabled = !newMatNameLen ||
                    std::any_of(materialsInProj.cbegin(),
                                materialsInProj.cend(), matWithSameNameExists);

    if (disabled) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Create")) {
      asset::MaterialAssetInfo newMatAssetInfo = {};
      newMatAssetInfo.materialType = core::MaterialType::lit;
      newMatAssetInfo.ambientColor = {1.0f, 1.0f, 1.0f, 1.0f};
      newMatAssetInfo.diffuseColor = {1.0f, 1.0f, 1.0f, 1.0f};
      newMatAssetInfo.specularColor = {1.0f, 1.0f, 1.0f, 1.0f};
      newMatAssetInfo.shininess = 1.0f;
      newMatAssetInfo.refractionIndex = 1.0f;

      asset::Asset newMatAsset;

      fs::path newMaterialAbsPath = project.getAbsolutePath(newMatName);
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
      for (int i = 0; i < materialsInProj.size(); ++i) {
        bool selected = selectedMaterialInd == i;

        if (ImGui::Selectable(materialPathStringPtrs[i], &selected)) {
          selectedMaterialInd = i;
          materialSelectionUpdated = true;
        }
      }

      ImGui::EndListBox();
    }

    ImGui::NewLine();

    if (materialSelectionUpdated && selectedMaterialInd >= 0) {
      fs::path const absolutePath =
          project.getAbsolutePath(materialsInProj[selectedMaterialInd]);
      asset::Asset matAsset;
      if (asset::loadAssetFromFile(absolutePath, matAsset) ||
          !matAsset.metadata) {
        if (asset::readMaterialAssetInfo(*matAsset.metadata,
                                         selectedMaterialAssetInfo)) {

        } else {
          OBS_LOG_ERR("Failed to load material asset info from asset on path " +
                      absolutePath.string());
        }
      } else {
        OBS_LOG_ERR("Failed to load asset from file on path " +
                    absolutePath.string());
      }

      selectedDiffuseTex = indexorDefault(
          texturesInProj, selectedMaterialAssetInfo.diffuseTexturePath, -1);
      selectedNormalMapTex = indexorDefault(
          texturesInProj, selectedMaterialAssetInfo.normalMapTexturePath, -1);

      selectedShader = indexorDefault(shadersInProj,
                                      selectedMaterialAssetInfo.shaderPath, -1);

      materialSelectionUpdated = false;
    }

    if (selectedMaterialInd >= 0) {
      ImGui::SeparatorText(materialPathStringPtrs[selectedMaterialInd]);

      if (ImGui::Combo("Material Type", &selectedMaterialType,
                       materialTypes.data(), materialTypes.size())) {
        selectedMaterialAssetInfo.materialType =
            static_cast<core::MaterialType>(selectedMaterialType);
      }

      if (ImGui::Combo("Shader", &selectedShader, shaderPathStringPtrs.data(),
                       shaderPathStringPtrs.size())) {
        selectedMaterialAssetInfo.shaderPath = shadersInProj[selectedShader];
      }

      int selectedDiffuseComboInd = selectedDiffuseTex + 1;
      if (ImGui::Combo("Diffuse Tex", &selectedDiffuseComboInd,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
        selectedDiffuseTex = selectedDiffuseComboInd - 1;
        if (selectedDiffuseTex >= 0) {
          selectedMaterialAssetInfo.diffuseTexturePath =
              texturesInProj[selectedDiffuseTex];
        }
      }

      int selectedNormalComboInd = selectedNormalMapTex + 1;
      if (ImGui::Combo("Normal Tex", &selectedNormalComboInd,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
        selectedNormalMapTex = selectedNormalComboInd - 1;
        if (selectedNormalMapTex >= 0) {
          selectedMaterialAssetInfo.normalMapTexturePath =
              texturesInProj[selectedNormalMapTex];
        }
      }

      if (ImGui::SliderFloat4("Ambient Color",
                              &selectedMaterialAssetInfo.ambientColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat4("Diffuse Color",
                              &selectedMaterialAssetInfo.diffuseColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat4("Specular Color",
                              &selectedMaterialAssetInfo.specularColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat("Shininess", &selectedMaterialAssetInfo.shininess,
                             1.0f, 256.0f)) {
      }

      if (ImGui::Checkbox("Transparent",
                          &selectedMaterialAssetInfo.transparent)) {
      }

      if (ImGui::Checkbox("Reflection",
                          &selectedMaterialAssetInfo.reflection)) {
      }

      if (ImGui::SliderFloat("RefractionIndex",
                             &selectedMaterialAssetInfo.refractionIndex, 1.0f,
                             5.0f)) {
      }

      if (ImGui::Checkbox("Uses Timer", &selectedMaterialAssetInfo.hasTimer)) {
      }

      if (ImGui::Button("Update")) {
        asset::Asset materialAsset;
        asset::packMaterial(selectedMaterialAssetInfo, {}, materialAsset);
        fs::path selectedMathAbsPath = project.getAbsolutePath(
            materialPathStringPtrs[selectedMaterialInd]);
        selectedMathAbsPath.replace_extension(".obsmat");
        asset::saveToFile(selectedMathAbsPath, materialAsset);
        assetListDirty = true;
      }
    }

    ImGui::EndTabItem();
  }
}

void textureEditorTab() {
  if (ImGui::BeginTabItem("Textures")) {
    static int selectedTextureInd = 1;
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

    if (!isInitialized && texturesInProj.size()) {
      loadTextureData(texturesInProj[selectedTextureInd - 1]);
      isInitialized = true;
    }

    if (texturesInProj.size()) {
      if (ImGui::Combo("Texture", &selectedTextureInd,
                       texturePathStringPtrs.data() + 1,
                       texturePathStringPtrs.size() - 1) &&
          selectedTextureInd) {
        loadTextureData(texturesInProj[selectedTextureInd]);
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

        asset::saveToFile(
            project.getAbsolutePath(texturesInProj[selectedTextureInd]),
            selectedTextureAsset);
      }
    }

    ImGui::EndTabItem();
  }
}

void projectTab(ObsidianEngine& engine, bool& engineStarted) {
  if (ImGui::BeginTabItem("Project")) {
    static char projPathBuf[maxPathSize];
    ImGui::InputText("Project Path", projPathBuf, std::size(projPathBuf));

    std::size_t projPathLen = std::strlen(projPathBuf);
    bool disabled = projPathLen == 0;

    if (disabled) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Open")) {
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

    if (project.getOpenProjectPath().empty()) {
      fs::path lastOpenProject = getLastOpenProject();

      if (!lastOpenProject.empty()) {
        if (ImGui::Button("Load last project")) {
          project.open(lastOpenProject);
          std::strncpy(projPathBuf, lastOpenProject.c_str(),
                       lastOpenProject.string().size());
          assetListDirty = true;
        }
        if (ImGui::Button("Load last project and run")) {
          if (project.open(lastOpenProject)) {
            std::strncpy(projPathBuf, lastOpenProject.c_str(),
                         lastOpenProject.string().size());
            assetListDirty = true;
            engineStarted = engine.init(sdl_wrapper::SDLBackend::instance(),
                                        project.getOpenProjectPath());
            openEngineTab = engineStarted;
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

void editor(SDL_Renderer& renderer, ImGuiIO& imguiIO, DataContext& context,
            ObsidianEngine& engine, bool& engineStarted) {
  if (assetListDirty) {
    refreshAssetLists();
    assetListDirty = false;
  }

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(imguiIO.DisplaySize);
  ImGui::SetNextWindowPos({0, 0});

  ImGui::Begin("EditorWindow");
  if (ImGui::BeginTabBar("EditorTabBar")) {

    projectTab(engine, engineStarted);
    if (!project.getOpenProjectPath().empty()) {
      engineTab(context.sceneData, engine, engineStarted);
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
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
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderPresent(&renderer);
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
  scene::GameObject& obj = ctx.scene.getState().gameObjects.emplace_back(
      ctx.vulkanRHI, ctx.resourceManager);
  scene::populateGameObject(gameObjectData, obj);
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
    if (project.getOpenProjectPath().empty()) {
      OBS_LOG_ERR("File dropped in but no project was open.");
      return;
    }

    fs::path dstPath = project.getAbsolutePath(srcPath.filename());
    dstPath.replace_extension("");

    if (engine.isInitialized()) {
      engine.getContext().taskExecutor.enqueue(
          task::TaskType::general, [&engine, srcPath, dstPath]() {
            obsidian::asset_converter::AssetConverter converter{
                engine.getContext().taskExecutor};
            converter.convertAsset(srcPath, dstPath);
            assetListDirty = true;
          });
    } else {
      // asset import
      static obsidian::task::TaskExecutor executor;
      static bool executorInitialized = false;

      if (!executorInitialized) {
        executor.initAndRun({{obsidian::task::TaskType::general,
                              std::thread::hardware_concurrency()}});
        executorInitialized = true;
      }

      executor.enqueue(task::TaskType::general, [srcPath, dstPath]() {
        obsidian::asset_converter::AssetConverter converter{executor};
        converter.convertAsset(srcPath, dstPath);
        assetListDirty = true;
      });
    }
  }
}

} /*namespace obsidian::editor*/
