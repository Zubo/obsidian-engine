#include <asset_converter/asset_converter.hpp>
#include <editor/data.hpp>
#include <editor/editor_windows.hpp>

#include <SDL2/SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>

#include <array>

namespace obsidian::editor {

constexpr const char* sceneWindowName = "Scene";

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

    constexpr std::size_t maxFilePathSize = 256;

    static char srcFilePath[maxFilePathSize];
    ImGui::InputText("Src file path", srcFilePath, std::size(srcFilePath));

    static char dstFilePath[maxFilePathSize];
    ImGui::InputText("Dst file path", dstFilePath, std::size(dstFilePath));

    if (ImGui::Button("Convert")) {
      obsidian::asset_converter::convertAsset(srcFilePath, dstFilePath);
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
    assetsTab();

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
