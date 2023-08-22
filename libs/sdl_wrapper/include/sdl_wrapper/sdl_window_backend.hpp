#pragma once

#include <window/window_impl_interface.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

struct SDL_Window;

namespace obsidian::sdl_wrapper {

class SDLWindowBackend : public window::interface::IWindowBackend {
public:
  using SDLWindowDeleter = void (*)(SDL_Window*);

  explicit SDLWindowBackend(
      std::unique_ptr<SDL_Window, SDLWindowDeleter> sdlWindow);

  virtual ~SDLWindowBackend() = default;

  void provideVulkanSurface(VkInstance vkInstance,
                            VkSurfaceKHR& outVkSurface) override;

  void
  pollEvents(std::vector<window::WindowEvent>& outWindowEvents) const override;

private:
  std::unique_ptr<SDL_Window, SDLWindowDeleter> _sdlWindowUnique;
};

} /*namespace obsidian::sdl_wrapper*/
