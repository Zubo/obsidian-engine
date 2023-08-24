#include <cstring>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/project/project.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <array>

namespace obsidian::editor {

constexpr const char* sceneWindowName = "Scene";
constexpr std::size_t maxPathSize = 256;
constexpr std::size_t maxFileSize = 64;
static obsidian::project::Project project;
namespace fs = std::filesystem;

void sceneTab(SceneData& sceneData) {
  if (ImGui::BeginTabItem("Scene")) {
    ImGui::SliderFloat("Sun direction X", &sceneData.sunlightDirection.x, -1.f,
                       1.f);
    ImGui::SliderFloat("Sun direction Y", &sceneData.sunlightDirection.y, -1.f,
                       1.f);
    ImGui::SliderFloat("Sun direction Z", &sceneData.sunlightDirection.z, -1.f,
                       1.f);

    ImGui::SliderFloat("Sun color r", &sceneData.sunlightColor.r, 0.f, 10.f);
    ImGui::SliderFloat("Sun color g", &sceneData.sunlightColor.g, 0.f, 10.f);
    ImGui::SliderFloat("Sun color b", &sceneData.sunlightColor.b, 0.f, 10.f);

    ImGui::SliderFloat("Ambient light color r", &sceneData.ambientColor.r, 0.f,
                       1.f);
    ImGui::SliderFloat("Ambient light color g", &sceneData.ambientColor.g, 0.f,
                       1.f);
    ImGui::SliderFloat("Ambient light color b", &sceneData.ambientColor.b, 0.f,
                       1.f);

    ImGui::EndTabItem();
  }
}

void assetsTab() {
  if (ImGui::BeginTabItem("Assets")) {
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
    }

    if (disabled) {
      ImGui::PopItemFlag();
      ImGui::PopStyleVar();
    }

    if (!project.getOpenProjectPath().empty()) {
      if (ImGui::BeginTabBar("EditorTabBar")) {
        assetsTab();
        ImGui::EndTabBar();
      }
    }

    ImGui::EndTabItem();
  }
}

void editor(SDL_Renderer& renderer, ImGuiIO& imguiIO, DataContext& context) {
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(imguiIO.DisplaySize);
  ImGui::SetNextWindowPos({0, 0});

  ImGui::Begin("EditorWindow");
  if (ImGui::BeginTabBar("EditorTabBar")) {
    sceneTab(context.sceneData);
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

} /*namespace obsidian::editor*/
