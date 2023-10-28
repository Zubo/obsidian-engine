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
#include <obsidian/asset/scene_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/editor/settings.hpp>
#include <obsidian/globals/file_extensions.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/scene/serialization.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

void refreshAssetLists() {
  texturesInProj = project.getAllFilesWithExtension(globals::textureAssetExt);
  texturePathStringPtrs.clear();

  texturePathStringPtrs.push_back("none");

  for (auto const& tex : texturesInProj) {
    texturePathStringPtrs.push_back(tex.c_str());
  }

  shaderPathStringPtrs.clear();
  shadersInProj = project.getAllFilesWithExtension(globals::shaderAssetExt);

  for (auto const& shad : shadersInProj) {
    shaderPathStringPtrs.push_back(shad.c_str());
  }

  materialPathStringPtrs.clear();
  materialsInProj = project.getAllFilesWithExtension(globals::materialAssetExt);

  for (auto const& mat : materialsInProj) {
    materialPathStringPtrs.push_back(mat.c_str());
  }

  meshesInProj = project.getAllFilesWithExtension(globals::meshAssetExt);

  meshesPathStringPtrs.clear();
  meshesPathStringPtrs.push_back("none");
  for (auto const& mesh : meshesInProj) {
    meshesPathStringPtrs.push_back(mesh.c_str());
  }
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
  if (!asset::loadFromFile(project.getAbsolutePath(scenePath), sceneAsset)) {
    OBS_LOG_ERR("Failed to load scene asset.");
    return;
  }

  asset::SceneAssetInfo sceneAssetInfo;

  if (!asset::readSceneAssetInfo(sceneAsset, sceneAssetInfo)) {
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

  if (!scene::deserializeScene(sceneJson, engine.getContext().resourceManager,
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
  asset::loadFromFile(project.getAbsolutePath(meshRelativePath), meshAsset);
  asset::readMeshAssetInfo(meshAsset, selectedGameObjMeshAssetInfo);

  selectedGameObjMats.clear();
  selectedGameObjMats.resize(
      selectedGameObjMeshAssetInfo.indexBufferSizes.size(), -1);

  for (std::size_t i = 0; i < selectedGameObjMats.size(); ++i) {
    if (selectedGameObj->materialResources.size() > i) {
      fs::path const materialRelativePath =
          selectedGameObj->materialResources[i]->getRelativePath();

      selectedGameObjMats[i] =
          indexorDefault(materialsInProj, materialRelativePath, -1);
    }

    if (selectedGameObjMats[i] < 0) {
      selectedGameObjMats[i] = indexorDefault(
          materialsInProj,
          selectedGameObjMeshAssetInfo.defaultMatRelativePaths[i], -1);
    }
  }
}

void selectGameObject(scene::GameObject& gameObject) {
  selectedGameObj = &gameObject;

  if (selectedGameObj->meshResource) {
    fs::path meshRelativePath =
        selectedGameObj->meshResource->getRelativePath();

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

  if (ImGui::TreeNode(gameObject.name.c_str())) {
    if (ImGui::IsItemClicked()) {
      selectGameObject(gameObject);
    }

    if (selected) {
      ImGui::PopStyleColor();
    }

    bool deleted = false;
    ImGui::PushID(gameObject.getId());
    if (ImGui::Button("+")) {
      gameObject.createChild();
    }

    ImGui::SameLine();

    if (ImGui::Button("-")) {
      scene::GameObject* const parent = gameObject.getParent();
      if (selectedGameObj == &gameObject) {
        selectedGameObj = nullptr;
      }

      if (parent) {
        parent->destroyChild(gameObject.getId());
        deleted = true;
      } else {
        auto const gameObjectIter = std::find_if(
            sceneState.gameObjects.cbegin(), sceneState.gameObjects.cend(),
            [&gameObject](scene::GameObject const& g) {
              return gameObject.getId() == g.getId();
            });

        if (gameObjectIter != sceneState.gameObjects.cend()) {
          sceneState.gameObjects.erase(gameObjectIter);
          deleted = true;
        }
      }
    }

    ImGui::PopID();

    if (!deleted) {
      for (scene::GameObject& gameObject : gameObject.getChildren()) {
        gameObjectHierarchy(gameObject, sceneState);
      }
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
            sceneState.gameObjects.emplace_back();
          }
          for (scene::GameObject& gameObject : sceneState.gameObjects) {
            gameObjectHierarchy(gameObject, sceneState);
          }
          ImGui::TreePop();
        }
      }

      if (selectedGameObj) {
        ImGui::NewLine();
        ImGui::SeparatorText("Game Object");

        char gameObjectName[maxGameObjectNameSize];
        std::strncpy(gameObjectName, selectedGameObj->name.c_str(),
                     selectedGameObj->name.size() + 1);

        ImGui::InputText("Name", gameObjectName, std::size(gameObjectName));

        selectedGameObj->name = gameObjectName;

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
          selectedGameObj->meshResource =
              meshDropdownSelectedIndex
                  ? &engine.getContext().resourceManager.getResource(
                        meshesInProj[meshDropdownSelectedIndex - 1])
                  : nullptr;

          selectedGameObj->materialResources.clear();
          for (int selectedMaterial : selectedGameObjMats) {
            selectedGameObj->materialResources.push_back(
                &engine.getContext().resourceManager.getResource(
                    project.getAbsolutePath(
                        materialsInProj[selectedMaterial])));
          }
        }

        if (disabled) {
          ImGui::PopItemFlag();
          ImGui::PopStyleVar();
        }

        ImGui::NewLine();
        ImGui::SeparatorText("Lights");

        if (!selectedGameObj->directionalLight) {
          if (ImGui::Button("Add directional light")) {
            selectedGameObj->directionalLight.emplace();
          }
        } else {
          ImGui::PushID("DirectionalLight");

          ImGui::SliderFloat3("Color",
                              reinterpret_cast<float*>(
                                  &selectedGameObj->directionalLight->color),
                              0.0f, 1.0f);

          ImGui::SliderFloat("Intensity",
                             &selectedGameObj->directionalLight->intensity,
                             0.0f, 10.0f);

          if (ImGui::Button("Remove")) {
            selectedGameObj->directionalLight.reset();
          }

          ImGui::PopID();
        }

        if (!selectedGameObj->spotlight) {
          if (ImGui::Button("Add spotlight")) {
            selectedGameObj->spotlight.emplace();
          }
        } else {
          ImGui::PushID("Spotlight");

          ImGui::SliderFloat3(
              "Color",
              reinterpret_cast<float*>(&selectedGameObj->spotlight->color),
              0.0f, 1.0f);

          ImGui::SliderFloat(
              "Intensity", &selectedGameObj->spotlight->intensity, 0.0f, 10.0f);

          ImGui::SliderFloat("Cutoff angle",
                             &selectedGameObj->spotlight->cutoffAngleRad, 0.01f,
                             3.14f);

          ImGui::SliderFloat("Fadeout angle",
                             &selectedGameObj->spotlight->fadeoutAngleRad,
                             selectedGameObj->spotlight->cutoffAngleRad, 3.14f);

          ImGui::SliderFloat("Linear attenuation",
                             &selectedGameObj->spotlight->linearAttenuation,
                             0.0f, 1.0f);

          ImGui::SliderFloat("Quadratic attenuation",
                             &selectedGameObj->spotlight->quadraticAttenuation,
                             0.0f, 1.0f);

          if (ImGui::Button("Remove")) {
            selectedGameObj->spotlight.reset();
          }

          ImGui::PopID();
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

void importTab() {
  if (ImGui::BeginTabItem("Import")) {
    static char srcFilePath[maxPathSize];
    ImGui::InputText("Src file path", srcFilePath, std::size(srcFilePath));

    static char dstFileName[maxPathSize];
    ImGui::InputText("Dst file name", dstFileName, std::size(dstFileName));

    if (ImGui::Button("Convert")) {
      fs::path destPath = project.getAbsolutePath(dstFileName);
      obsidian::asset_converter::convertAsset(srcFilePath, destPath);
    }

    ImGui::EndTabItem();
  }
}

void materialCreatorTab() {
  static bool isOpen = false;

  if (ImGui::BeginTabItem("Material Creator")) {
    static int selectedMaterialType = static_cast<int>(core::MaterialType::lit);
    static int selectedDiffuseTex = 0;
    static int selectedNormalMapTex = 0;
    static int selectedShader = 0;
    static float selectedShininess = 16.0f;
    static bool selectedMatTransparent = false;
    static glm::vec4 selectedAmbientColor = {1.0f, 1.0f, 1.0f, 1.0f};
    static glm::vec4 selectedDiffuseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    static glm::vec4 selectedSpecularColor = {1.0f, 1.0f, 1.0f, 1.0f};

    bool canCreateMat = true;
    if (!texturesInProj.size()) {
      ImGui::Text("No textures in the project");
      canCreateMat = false;
    }
    if (!shadersInProj.size()) {
      ImGui::Text("No shaders in the project");
      canCreateMat = false;
    }

    if (canCreateMat) {
      static char matName[maxFileNameSize];
      ImGui::InputText("Material name", matName, std::size(matName));

      if (ImGui::Combo("Material Type", &selectedMaterialType,
                       materialTypes.data(), materialTypes.size())) {
      }

      if (ImGui::Combo("Shader", &selectedShader, shaderPathStringPtrs.data(),
                       shaderPathStringPtrs.size())) {
      }

      if (ImGui::Combo("Diffuse Tex", &selectedDiffuseTex,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
      }

      if (ImGui::Combo("Normal Tex", &selectedNormalMapTex,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
      }

      if (ImGui::SliderFloat4("Ambient Color", &selectedAmbientColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat4("Diffuse Color", &selectedDiffuseColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat4("Specular Color", &selectedSpecularColor.r, 0.0f,
                              1.0f)) {
      }

      if (ImGui::SliderFloat("Shininess", &selectedShininess, 1.0f, 256.0f)) {
      }

      if (ImGui::Checkbox("Transparent", &selectedMatTransparent)) {
      }

      std::size_t matNameLen = std::strlen(matName);
      bool disabled = matNameLen == 0;

      if (disabled) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
      }

      if (ImGui::Button("Create")) {
        asset::MaterialAssetInfo mtlAssetInfo;
        mtlAssetInfo.compressionMode = asset::CompressionMode::none;
        mtlAssetInfo.materialType =
            static_cast<core::MaterialType>(selectedMaterialType);
        mtlAssetInfo.shaderPath = shaderPathStringPtrs[selectedShader];
        mtlAssetInfo.ambientColor = selectedAmbientColor;
        mtlAssetInfo.diffuseColor = selectedDiffuseColor;
        mtlAssetInfo.specularColor = selectedSpecularColor;
        mtlAssetInfo.transparent = selectedMatTransparent;

        if (selectedDiffuseTex > 0) {
          mtlAssetInfo.diffuseTexturePath =
              texturesInProj[selectedDiffuseTex - 1];
        }

        if (selectedNormalMapTex > 0) {
          mtlAssetInfo.normalMapTexturePath =
              texturesInProj[selectedNormalMapTex - 1];
        }

        mtlAssetInfo.shininess = selectedShininess;

        asset::Asset materialAsset;
        asset::packMaterial(mtlAssetInfo, {}, materialAsset);
        fs::path matPath = project.getAbsolutePath(matName);
        matPath.replace_extension(".obsmat");
        asset::saveToFile(matPath, materialAsset);
        assetListDirty = true;
      }

      if (disabled) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
      }
    } else {
      isOpen = false;
    }

    ImGui::EndTabItem();
  } else {
    isOpen = false;
  }
}

void textureEditorTab() {
  if (ImGui::BeginTabItem("Textures")) {
    static int selectedTextureInd = 1;
    static int selectedTextureFormat;
    static bool isInitialized = false;

    auto const loadTextureData = [](fs::path const& texturePath) {
      asset::Asset textureAsset;
      asset::loadFromFile(project.getAbsolutePath(texturePath), textureAsset);
      asset::TextureAssetInfo textureAssetInfo;
      asset::readTextureAssetInfo(textureAsset, textureAssetInfo);
      selectedTextureFormat = static_cast<int>(textureAssetInfo.format);
    };

    if (!isInitialized && texturesInProj.size()) {
      loadTextureData(texturesInProj[selectedTextureInd - 1]);
      isInitialized = true;
    }

    if (texturesInProj.size()) {
      if (ImGui::Combo("Texture", &selectedTextureInd,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
        loadTextureData(texturesInProj[selectedTextureInd - 1]);
      }

      if (ImGui::Combo("Format", &selectedTextureFormat, textureTypes.data(),
                       (int)textureTypes.size())) {
      }

      if (ImGui::Button("Save")) {
        asset::Asset textureAsset;
        asset::loadFromFile(
            project.getAbsolutePath(texturesInProj[selectedTextureInd - 1]),
            textureAsset);
        asset::TextureAssetInfo textureAssetInfo;
        asset::readTextureAssetInfo(textureAsset, textureAssetInfo);

        std::vector<char> textureData;
        textureData.resize(textureAssetInfo.unpackedSize);

        asset::unpackAsset(textureAssetInfo, textureAsset.binaryBlob.data(),
                           textureAsset.binaryBlob.size(), textureData.data());

        asset::Asset outAsset;
        textureAssetInfo.format =
            static_cast<core::TextureFormat>(selectedTextureFormat);
        asset::packTexture(textureAssetInfo, textureData.data(), outAsset);
        asset::saveToFile(
            project.getAbsolutePath(texturesInProj[selectedTextureInd - 1]),
            outAsset);
      }
    }

    ImGui::EndTabItem();
  }
}

void projectTab(ObsidianEngine& engine, bool& engineStarted) {
  if (ImGui::BeginTabItem("Project")) {
    fs::path projPath = project.getOpenProjectPath();

    static char projPathBuf[maxPathSize];
    ImGui::InputText("Project Path", projPathBuf, std::size(projPathBuf));

    std::size_t projPathLen = std::strlen(projPathBuf);
    bool disabled = projPathLen == 0;

    if (disabled) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Open")) {
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
            openEngineTab = true;
          }
        }
      }
    } else {
      if (ImGui::BeginTabBar("EditorTabBar")) {
        materialCreatorTab();
        textureEditorTab();
        importTab();
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

void fileDropped(const char* file) {
  if (project.getOpenProjectPath().empty()) {
    OBS_LOG_ERR("File dropped in but no project was open.");
    return;
  }

  fs::path const srcPath{file};
  fs::path destPath = project.getAbsolutePath(srcPath.filename());
  destPath.replace_extension("");
  obsidian::asset_converter::convertAsset(srcPath, destPath);

  assetListDirty = true;
}

} /*namespace obsidian::editor*/
