#include "obsidian/core/logging.hpp"
#include <filesystem>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/editor/settings.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <iostream>

namespace obsidian::editor {

constexpr const char* sceneWindowName = "Scene";
constexpr std::size_t maxPathSize = 256;
constexpr std::size_t maxFileSize = 64;
static obsidian::project::Project project;
namespace fs = std::filesystem;

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

      if (ImGui::CollapsingHeader("Scene")) {
        if (ImGui::TreeNode("Object Hierarchy")) {
          if (ImGui::TreeNode("Game obj")) {
            ImGui::TreePop();
            if (ImGui::Button("+")) {
              // Add object
            }
          }
          ImGui::TreePop();
          if (ImGui::Button("+")) {
            // Add object
          }
        }
      }

    } else {
      if (ImGui::Button("Start Engine")) {
        engineStarted = true;
        engine.init(sdl_wrapper::SDLBackend::instance());
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
  static std::vector<fs::path> texturesInProj;
  static std::vector<char const*> texturePathStringPtrs;
  static int selectedItem = 0;
  if (ImGui::BeginTabItem("Material Creator")) {
    bool justOpened = !isOpen;
    isOpen = true;

    if (justOpened) {
      texturesInProj = project.getAllFilesWithExtension(".obstex");
      texturePathStringPtrs.clear();

      for (auto const& tex : texturesInProj) {
        texturePathStringPtrs.push_back(tex.c_str());
      }
    }

    if (ImGui::Combo("Mat Tex", &selectedItem, texturePathStringPtrs.data(),
                     texturePathStringPtrs.size())) {
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
