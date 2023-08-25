/*
 *
 *
 *  Needs refactor
 *
 *
 */

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/editor/settings.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <filesystem>
#include <iostream>

namespace obsidian::editor {

constexpr const char* sceneWindowName = "Scene";
constexpr std::size_t maxPathSize = 256;
constexpr std::size_t maxFileNameSize = 64;
constexpr std::size_t maxGameObjectNameSize = 64;
static obsidian::project::Project project;
namespace fs = std::filesystem;

static scene::GameObject* selectedGameObject = nullptr;

void gameObjectHierarchy(scene::GameObject& gameObject,
                         scene::SceneState& sceneState) {
  bool selected = &gameObject == selectedGameObject;

  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  }
  if (ImGui::TreeNode(gameObject.name.c_str())) {
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
      if (selectedGameObject == &gameObject) {
        selectedGameObject = nullptr;
      }

      if (parent) {
        parent->destroyChild(gameObject.getId());
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

    ImGui::SameLine();

    if (ImGui::Button("Select")) {
      selectedGameObject = &gameObject;
    }

    ImGui::PopID();

    if (!deleted) {
      for (scene::GameObject& gameObject : gameObject.getChildren()) {
        gameObjectHierarchy(gameObject, sceneState);
      }
    }

    ImGui::TreePop();
  } else if (selected) {
    ImGui::PopStyleColor();
  }
}

void engineTab(SceneData& sceneData, ObsidianEngine& engine,
               bool& engineStarted) {
  if (ImGui::BeginTabItem("Engine")) {
    if (engineStarted) {
      if (ImGui::CollapsingHeader("Global Scene Params")) {
        ImGui::SliderFloat("Sun direction X", &sceneData.sunlightDirection.x,
                           -1.f, 1.f);
        ImGui::SliderFloat("Sun direction Y", &sceneData.sunlightDirection.y,
                           -1.f, 1.f);
        ImGui::SliderFloat("Sun direction Z", &sceneData.sunlightDirection.z,
                           -1.f, 1.f);

        ImGui::SliderFloat("Sun color r", &sceneData.sunlightColor.r, 0.f,
                           10.f);
        ImGui::SliderFloat("Sun color g", &sceneData.sunlightColor.g, 0.f,
                           10.f);
        ImGui::SliderFloat("Sun color b", &sceneData.sunlightColor.b, 0.f,
                           10.f);

        ImGui::SliderFloat("Ambient light color r", &sceneData.ambientColor.r,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color g", &sceneData.ambientColor.g,
                           0.f, 1.f);
        ImGui::SliderFloat("Ambient light color b", &sceneData.ambientColor.b,
                           0.f, 1.f);
      }

      scene::SceneState& sceneState = engine.getContext().scene.getState();

      if (ImGui::CollapsingHeader("Scene")) {
        if (ImGui::TreeNode("Object Hierarchy")) {
          if (ImGui::Button("+")) {
            sceneState.gameObjects.emplace_back();
          }
          for (scene::GameObject& gameObject : sceneState.gameObjects) {
            gameObjectHierarchy(gameObject, sceneState);
          }
          ImGui::TreePop();
        }
      }

      if (ImGui::CollapsingHeader("Game Object")) {
        if (selectedGameObject) {
          char gameObjectName[maxGameObjectNameSize];
          std::strncpy(gameObjectName, selectedGameObject->name.c_str(),
                       selectedGameObject->name.size() + 1);

          ImGui::InputText("Name", gameObjectName, std::size(gameObjectName));

          selectedGameObject->name = gameObjectName;

          glm::vec3 pos = selectedGameObject->getPosition();
          ImGui::InputScalarN("Position", ImGuiDataType_Float, &pos, 3);
          selectedGameObject->setPosition(pos);
        }
      }
    } else {
      if (ImGui::Button("Start Engine")) {
        engine.init(sdl_wrapper::SDLBackend::instance());
        engineStarted = true;
      }
    }
    ImGui::EndTabItem();
  }
}

void assetsTab() {
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
    static int selectedMaterialType = 0;
    static char const* materialTypes[] = {"lit", "unlit"};
    static int selectedAlbedoTex = 0;
    static std::vector<fs::path> texturesInProj;
    static int selectedVertexShad = 0;
    static int selectedFragmentShad = 0;
    static std::vector<fs::path> shadersInProj;
    static std::vector<char const*> texturePathStringPtrs;
    static std::vector<char const*> shaderPathStringPtrs;

    bool justOpened = !isOpen;
    isOpen = true;

    if (justOpened) {
      texturesInProj = project.getAllFilesWithExtension(".obstex");
      texturePathStringPtrs.clear();

      for (auto const& tex : texturesInProj) {
        texturePathStringPtrs.push_back(tex.c_str());
      }

      shaderPathStringPtrs.clear();
      shadersInProj = project.getAllFilesWithExtension(".obsshad");

      for (auto const& shad : shadersInProj) {
        shaderPathStringPtrs.push_back(shad.c_str());
      }
    }

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

      if (ImGui::Combo("Material Type", &selectedMaterialType, materialTypes,
                       std::size(materialTypes))) {
      }

      if (ImGui::Combo("Vertex Shader", &selectedVertexShad,
                       shaderPathStringPtrs.data(),
                       shaderPathStringPtrs.size())) {
      }

      if (ImGui::Combo("Fragment Shader", &selectedFragmentShad,
                       shaderPathStringPtrs.data(),
                       shaderPathStringPtrs.size())) {
      }

      if (ImGui::Combo("Albedo Tex", &selectedAlbedoTex,
                       texturePathStringPtrs.data(),
                       texturePathStringPtrs.size())) {
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
        mtlAssetInfo.vertexShaderPath =
            shaderPathStringPtrs[selectedVertexShad];
        mtlAssetInfo.fragmentShaderPath =
            shaderPathStringPtrs[selectedFragmentShad];
        mtlAssetInfo.albedoTexturePath =
            texturePathStringPtrs[selectedAlbedoTex];

        asset::Asset materialAsset;
        asset::packMaterial(mtlAssetInfo, {}, materialAsset);
        fs::path matPath = matName;
        matPath.replace_extension(".obsmat");
        asset::saveToFile(project.getAbsolutePath(matPath), materialAsset);
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

void projectTab() {
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
        }
      }
    } else {
      if (ImGui::BeginTabBar("EditorTabBar")) {
        assetsTab();
        materialCreatorTab();
        ImGui::EndTabBar();
      }
    }

    ImGui::EndTabItem();
  }
}

void editor(SDL_Renderer& renderer, ImGuiIO& imguiIO, DataContext& context,
            ObsidianEngine& engine, bool& engineStarted) {
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(imguiIO.DisplaySize);
  ImGui::SetNextWindowPos({0, 0});

  ImGui::Begin("EditorWindow");
  if (ImGui::BeginTabBar("EditorTabBar")) {

    engineTab(context.sceneData, engine, engineStarted);
    projectTab();

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
}

} /*namespace obsidian::editor*/
