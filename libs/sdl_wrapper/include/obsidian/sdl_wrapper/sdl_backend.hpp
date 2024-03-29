#pragma once

#include <obsidian/rhi/rhi.hpp>
#include <obsidian/window/window_backend.hpp>

#include <SDL2/SDL_events.h>

#include <vector>

namespace obsidian::sdl_wrapper {

class SDLBackend : public window::interface::IWindowBackendProvider {
public:
  using window::interface::IWindowBackendProvider::CreateWindowParams;

  static SDLBackend& instance();

  virtual ~SDLBackend();

  void init();
  bool isInitialized();
  void pollEvents();
  std::vector<SDL_Event> const& getPolledEvents() const;

  std::unique_ptr<window::interface::IWindowBackend>
  createWindow(CreateWindowParams const& params,
               rhi::RHIBackends backend) const override;

private:
  SDLBackend() = default;

  bool _initialized = false;
  std::vector<SDL_Event> _currentFrameEvents;
};

} /*namespace obsidian::sdl_wrapper*/
