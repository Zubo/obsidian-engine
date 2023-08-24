#include <obsidian/rhi/rhi.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/sdl_wrapper/sdl_window_backend.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window_events.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>

#include <cassert>
#include <vector>

using namespace obsidian;
using namespace obsidian::sdl_wrapper;

SDLWindowBackend::SDLWindowBackend(
    std::unique_ptr<SDL_Window, SDLWindowDeleter> sdlWindow,
    rhi::RHIBackends backend)
    : _sdlWindowUnique{std::move(sdlWindow)} {
  assert(backend == rhi::RHIBackends::vulkan &&
         "Currently only Vulkan backend is supported");
}

void SDLWindowBackend::provideSurface(rhi::RHI& rhi) const {
  vk_rhi::VulkanRHI* vulkanRhi = dynamic_cast<vk_rhi::VulkanRHI*>(&rhi);

  assert(vulkanRhi);

  VkSurfaceKHR surface;
  SDL_Vulkan_CreateSurface(_sdlWindowUnique.get(), vulkanRhi->getInstance(),
                           &surface);
  vulkanRhi->setSurface(surface);
}

void SDLWindowBackend::pollEvents(
    std::vector<window::WindowEvent>& outWindowEvents) const {

  Uint32 const windowID = SDL_GetWindowID(_sdlWindowUnique.get());

  std::vector<SDL_Event> const& polledEvents =
      SDLBackend::instance().getPolledEvents();

  for (SDL_Event const& e : polledEvents) {
    window::WindowEvent outEvent;

    switch (e.type) {
    case SDL_KEYDOWN:
      if (e.key.windowID == windowID) {
        outEvent.keyDownEvent =
            window::KeyDownEvent{window::WindowEventType::KeyDown,
                                 static_cast<core::KeyCode>(e.key.keysym.sym)};
      }
      break;
    case SDL_KEYUP:
      if (e.key.windowID == windowID) {
        outEvent.keyUpEvent =
            window::KeyUpEvent{window::WindowEventType::KeyUp,
                               static_cast<core::KeyCode>(e.key.keysym.sym)};
      }
      break;
    case SDL_MOUSEMOTION:
      if (e.motion.windowID == windowID) {
        outEvent.mouseMotionEvent = window::MouseMotionEvent{
            window::WindowEventType::MouseMotion, e.motion.xrel, e.motion.yrel};
      }
      break;
    case SDL_WINDOWEVENT:
      if (e.window.windowID == windowID) {
        if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
            e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          outEvent.windowResized =
              window::WindowResizedEvent{window::WindowEventType::WindowResized,
                                         e.window.data1, e.window.data2};
        } else if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
          outEvent.shouldQuitEvent =
              window::ShouldQuitEvent{window::WindowEventType::ShouldQuit};
        } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
          outEvent.focusGained = window::FocusGainedEvent{
              window::WindowEventType::FocusGainedEvent};
        } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
          outEvent.focusLost =
              window::FocusLostEvent{window::WindowEventType::FocusLostEvent};
        }
      }
    }

    outWindowEvents.push_back(outEvent);
  }
}
