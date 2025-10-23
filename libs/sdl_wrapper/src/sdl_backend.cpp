#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/sdl_wrapper/sdl_window_backend.hpp>
#include <obsidian/window/window_backend.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <memory>

using namespace obsidian;
using namespace obsidian::sdl_wrapper;

SDLBackend& SDLBackend::instance() {
  static SDLBackend instance;

  if (!instance._initialized) {
    instance.init();
  }

  return instance;
}

SDLBackend::~SDLBackend() { SDL_Quit(); }

void SDLBackend::init() {
  if (SDL_WasInit(SDL_INIT_VIDEO)) {
    OBS_LOG_MSG("SDL with flag SDL_INIT_VIDEO already initialized.");
    return;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    OBS_LOG_ERR(std::string("Error: ") + SDL_GetError());
  }

  _initialized = true;
}

bool SDLBackend::isInitialized() { return _initialized; }

void SDLBackend::pollEvents() {
  _currentFrameEvents.clear();

  SDL_Event e;

  while (SDL_PollEvent(&e)) {
    _currentFrameEvents.push_back(e);
  }
}

std::vector<SDL_Event> const& SDLBackend::getPolledEvents() const {
  return _currentFrameEvents;
}

std::unique_ptr<window::interface::IWindowBackend>
SDLBackend::createWindow(const CreateWindowParams& params,
                         rhi::RHIBackends backend) const {
  assert(backend == rhi::RHIBackends::vulkan &&
         "Currently only Vulkan backend is supported.");

  SDL_WindowFlags const flags = static_cast<SDL_WindowFlags>(
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
      SDL_WINDOW_HIGH_PIXEL_DENSITY);

  using UniquePtr =
      std::unique_ptr<SDL_Window, SDLWindowBackend::SDLWindowDeleter>;
  UniquePtr sdlWindowUnique =
      UniquePtr(SDL_CreateWindow(params.title.c_str(), params.width,
                                 params.height, flags),
                [](SDL_Window* w) {
                  if (w)
                    SDL_DestroyWindow(w);
                });

  return std::make_unique<SDLWindowBackend>(std::move(sdlWindowUnique),
                                            rhi::RHIBackends::vulkan);
}
