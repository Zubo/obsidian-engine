
#include <core/logging.hpp>
#include <editor/data.hpp>
#include <editor/editor_windows.hpp>
#include <obsidian_engine/obsidian_engine.hpp>
#include <scene/scene.hpp>
#include <sdl_wrapper/sdl_backend.hpp>
#include <vk_rhi/vk_rhi.hpp>
#include <vk_rhi/vk_rhi_input.hpp>
#include <window/window.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL_events.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

int main(int, char**) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS |
               SDL_INIT_GAMECONTROLLER) != 0) {
    OBS_LOG_ERR(std::string("Error: ") + SDL_GetError());
    return -1;
  }

  // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

  SDL_DisplayMode displayMode;
  SDL_GetCurrentDisplayMode(0, &displayMode);
  Uint32 const screenCenterX = displayMode.w / 2;
  Uint32 const screenCenterY = displayMode.h / 2;

  constexpr Uint32 editorWindowWidth = 400;
  constexpr Uint32 editorWindowHeight = 800;
  Uint32 const editorWindowXPos = screenCenterX + 500 + 10;
  Uint32 const editorWindowYPos = screenCenterY - editorWindowHeight / 2;

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags editorWindowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* editorWindow = SDL_CreateWindow(
      "Obsidian Editor", editorWindowXPos, editorWindowYPos, editorWindowWidth,
      editorWindowHeight, editorWindowFlags);
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

  obsidian::ObsidianEngine engine;
  auto& sdlBackend = obsidian::sdl_wrapper::SDLBackend::instance();

  engine.init(sdlBackend);

  obsidian::ObsidianEngineContext& engineContext = engine.getContext();

  obsidian::editor::DataContext dataContext;

  bool shouldQuit = false;

  while (!shouldQuit && !engineContext.window.shouldQuit()) {
    ZoneScoped;

    sdlBackend.pollEvents();
    std::vector<SDL_Event> const& polledEvenets = sdlBackend.getPolledEvents();

    for (SDL_Event const& e : polledEvenets) {
      ImGui_ImplSDL2_ProcessEvent(&e);

      if (e.type == SDL_QUIT) {
        shouldQuit = true;
      }
      if (e.type == SDL_WINDOWEVENT &&
          e.window.event == SDL_WINDOWEVENT_CLOSE) {
        shouldQuit = true;
      }
    }

    engineContext.window.pollEvents();
    obsidian::editor::editor(*editorUIRenderer, io, dataContext);
    obsidian::scene::SceneState& sceneState = engineContext.scene.getState();
    sceneState.ambientColor = dataContext.sceneData.ambientColor;
    sceneState.sunColor = dataContext.sceneData.sunlightColor;
    sceneState.sunDirection = dataContext.sceneData.sunlightDirection;

    engine.processFrame();

    FrameMark;
  }

  // Cleanup
  engine.cleanup();
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(editorUIRenderer);
  SDL_DestroyWindow(editorWindow);

  return 0;
}
