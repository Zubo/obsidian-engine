
#include <obsidian/core/logging.hpp>
#include <obsidian/editor/data.hpp>
#include <obsidian/editor/editor_windows.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/platform/environment.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window.hpp>

#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL_events.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>

#include <atomic>

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

int main(int argc, char const** argv) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS |
               SDL_INIT_GAMECONTROLLER) != 0) {
    OBS_LOG_ERR(SDL_GetError());
    return -1;
  }

  // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

  SDL_DisplayMode displayMode;
  SDL_GetCurrentDisplayMode(0, &displayMode);

  constexpr Uint32 editorWindowWidth = 400;
  constexpr Uint32 editorWindowHeight = 800;

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags editorWindowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* editorWindow = SDL_CreateWindow(
      "Obsidian Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      editorWindowWidth, editorWindowHeight, editorWindowFlags);
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

  using namespace obsidian;
  ObsidianEngine engine;
  auto& sdlBackend = sdl_wrapper::SDLBackend::instance();

  ObsidianEngineContext& engineContext = engine.getContext();

  editor::DataContext dataContext;

  std::atomic_flag shouldQuit;

  while (!shouldQuit.test()) {
    ZoneScoped;

    sdlBackend.pollEvents();
    std::vector<SDL_Event> const& polledEvenets = sdlBackend.getPolledEvents();

    for (SDL_Event const& e : polledEvenets) {
      ImGui_ImplSDL2_ProcessEvent(&e);

      if (e.type == SDL_QUIT || (e.type == SDL_WINDOWEVENT &&
                                 e.window.event == SDL_WINDOWEVENT_CLOSE)) {
        shouldQuit.test_and_set();
        engine.requestShutdown();
      }

      if (e.type == SDL_DROPFILE) {
        if (e.drop.windowID == SDL_GetWindowID(editorWindow)) {
          editor::fileDropped(e.drop.file, engine);
        }
      }
    }

    editor::begnEditorFrame(io);
    editor::editorWindow(*editorUIRenderer, io, dataContext, engine);

    engineContext.scene.setAmbientColor(dataContext.sceneData.ambientColor);

    if (engine.isInitialized() && !shouldQuit.test()) {
      engineContext.window.pollEvents();
      engine.waitFrameProcessed();
      engine.prepareRenderData();
    }

    editor::endEditorFrame(*editorUIRenderer, io);

    FrameMark;
  }

  // Cleanup
  if (engine.isInitialized()) {
    engine.cleanup();
  }

  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(editorUIRenderer);
  SDL_DestroyWindow(editorWindow);

  return 0;
}
