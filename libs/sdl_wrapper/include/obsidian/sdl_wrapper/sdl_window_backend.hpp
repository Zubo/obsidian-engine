#pragma once

#include <obsidian/rhi/rhi.hpp>
#include <obsidian/window/window_backend.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

struct SDL_Window;

namespace obsidian::sdl_wrapper {

class SDLWindowBackend : public window::interface::IWindowBackend {
public:
  using SDLWindowDeleter = void (*)(SDL_Window*);

  explicit SDLWindowBackend(
      std::unique_ptr<SDL_Window, SDLWindowDeleter> sdlWindow,
      rhi::RHIBackends backend);

  virtual ~SDLWindowBackend() = default;

  void provideSurface(rhi::RHI& rhi) const override;

  void
  pollEvents(std::vector<window::WindowEvent>& outWindowEvents) const override;

private:
  std::unique_ptr<SDL_Window, SDLWindowDeleter> _sdlWindowUnique;
};

} /*namespace obsidian::sdl_wrapper*/
