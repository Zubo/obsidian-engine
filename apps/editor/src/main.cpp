
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

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>

#include <atomic>

#if !SDL_VERSION_ATLEAST(3, 0, 0)
#error This backend requires SDL 3.0.0+ because of SDL_RenderGeometry() function
#endif

int main(int argc, char const** argv) {
  // Setup SDL
  SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    OBS_LOG_ERR(SDL_GetError());
    return -1;
  }

  // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

  constexpr Uint32 editorWindowWidth = 400;
  constexpr Uint32 editorWindowHeight = 800;

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags editorWindowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Window* editorWindow =
      SDL_CreateWindow("Obsidian Editor", editorWindowWidth, editorWindowHeight,
                       editorWindowFlags);
  SDL_Renderer* editorUIRenderer = SDL_CreateRenderer(editorWindow, NULL);
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

  ImGui_ImplSDL3_InitForSDLRenderer(editorWindow, editorUIRenderer);
  ImGui_ImplSDLRenderer3_Init(editorUIRenderer);

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
      ImGui_ImplSDL3_ProcessEvent(&e);

      if (e.type == SDL_EVENT_QUIT ||
          e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        shouldQuit.test_and_set();
        engine.requestShutdown();
      }

      if (e.type == SDL_EVENT_DROP_FILE) {
        if (e.drop.windowID == SDL_GetWindowID(editorWindow)) {
          editor::fileDropped(e.drop.data, engine);
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

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(editorUIRenderer);
  SDL_DestroyWindow(editorWindow);

  return 0;
}
