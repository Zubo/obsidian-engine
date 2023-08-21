
#include "editor/data.hpp"
#include "editor/editor_windows.hpp"
#include <SDL_events.h>
#include <vk_rhi/vk_rhi.hpp>

#include <SDL2/SDL.h>
#include <SDL_video.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>

#include <iostream>

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

int main(int, char**) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS |
               SDL_INIT_GAMECONTROLLER) != 0) {
    std::cout << "Error: " << SDL_GetError() << std::endl;
    return -1;
  }

  // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags editorWindowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* editorWindow =
      SDL_CreateWindow("Obsidian Editor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 720, editorWindowFlags);
  SDL_Renderer* editorUIRenderer = SDL_CreateRenderer(
      editorWindow, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (editorUIRenderer == nullptr) {
    SDL_Log("Error creating SDL_Renderer!");
    return 0;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForSDLRenderer(editorWindow, editorUIRenderer);
  ImGui_ImplSDLRenderer2_Init(editorUIRenderer);

  SDL_WindowFlags engineWindowFlags = static_cast<SDL_WindowFlags>(
      SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  SDL_Window* engineWindow =
      SDL_CreateWindow("Obsidian Engine", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1200, 800, engineWindowFlags);

  obsidian::vk_rhi::VulkanRHI vulkanRHI;
  vulkanRHI.init(*engineWindow);

  obsidian::editor::DataContext dataContext;

  bool shouldQuit = false;
  while (!shouldQuit) {
    SDL_Event event;
    std::size_t sizeCnt = 0;
    while (SDL_PollEvent(&event)) {
      ZoneScoped;

      ImGui_ImplSDL2_ProcessEvent(&event);

      if (event.type == SDL_QUIT) {
        shouldQuit = true;
      }
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE) {
        shouldQuit = true;
      } else {
        vulkanRHI.handleEvents(event);
      }
    }

    obsidian::editor::editor(*editorUIRenderer, io, dataContext);
    vulkanRHI.setSceneParams(dataContext.sceneData.ambientColor,
                             dataContext.sceneData.sunlightDirection,
                             dataContext.sceneData.sunlightColor);

    vulkanRHI.draw();

    FrameMark;
  }

  // Cleanup
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(editorUIRenderer);
  SDL_DestroyWindow(editorWindow);
  SDL_Quit();

  return 0;
}
