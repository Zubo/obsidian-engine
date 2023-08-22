#include "sdl_wrapper/sdl_window_backend.hpp"
#include "window/window_impl_interface.hpp"
#include <cstdlib>
#include <memory>
#include <sdl_wrapper/sdl_backend.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

#include <functional>
#include <iostream>

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
  if (int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    std::cout << "Error: " << SDL_GetError() << std::endl;
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
SDLBackend::createWindow(const CreateWindowParams& params) const {
  SDL_WindowFlags const flags = static_cast<SDL_WindowFlags>(
      SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  using UniquePtr =
      std::unique_ptr<SDL_Window, SDLWindowBackend::SDLWindowDeleter>;
  UniquePtr sdlWindowUnique =
      UniquePtr(SDL_CreateWindow(params.title.c_str(), params.posX, params.posY,
                                 params.width, params.height, flags),
                [](SDL_Window* w) {
                  if (w)
                    SDL_DestroyWindow(w);
                });

  return std::make_unique<SDLWindowBackend>(std::move(sdlWindowUnique));
}
