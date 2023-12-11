#include <obsidian/core/keycode.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/sdl_wrapper/sdl_window_backend.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window_events.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL_mouse.h>

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

core::MouseButtonType getMouseButtonType(int sdlButtonType) {
  switch (sdlButtonType) {
  case SDL_BUTTON_LEFT:
    return core::MouseButtonType::left;
  case SDL_BUTTON_RIGHT:
    return core::MouseButtonType::right;
  case SDL_BUTTON_MIDDLE:
    return core::MouseButtonType::middle;
  case SDL_BUTTON_X1:
    return core::MouseButtonType::x1;
  case SDL_BUTTON_X2:
    return core::MouseButtonType::x2;
  default:
    return core::MouseButtonType::unknown;
  }
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
            window::KeyDownEvent{window::WindowEventType::keyDown,
                                 static_cast<core::KeyCode>(e.key.keysym.sym)};
      }
      break;
    case SDL_KEYUP:
      if (e.key.windowID == windowID) {
        outEvent.keyUpEvent =
            window::KeyUpEvent{window::WindowEventType::keyUp,
                               static_cast<core::KeyCode>(e.key.keysym.sym)};
      }
      break;
    case SDL_MOUSEMOTION:
      if (e.motion.windowID == windowID) {
        outEvent.mouseMotionEvent = window::MouseMotionEvent{
            window::WindowEventType::mouseMotion, e.motion.xrel, e.motion.yrel};
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (e.button.windowID == windowID) {
        core::MouseButtonType const button =
            getMouseButtonType(e.button.button);
        outEvent.mouseButtonDownEvent = window::MouseButtonDownEvent{
            window::WindowEventType::mouseButtonDown, button};
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (e.button.windowID == windowID) {
        core::MouseButtonType const button =
            getMouseButtonType(e.button.button);
        outEvent.mouseButtonUpEvent = window::MouseButtonUpEvent{
            window::WindowEventType::mouseButtonUp, button};
      }
      break;
    case SDL_WINDOWEVENT:
      if (e.window.windowID == windowID) {
        if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
            e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          outEvent.windowResized =
              window::WindowResizedEvent{window::WindowEventType::windowResized,
                                         e.window.data1, e.window.data2};
        } else if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
          outEvent.shouldQuitEvent =
              window::ShouldQuitEvent{window::WindowEventType::shouldQuit};
        } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
          outEvent.focusGained = window::FocusGainedEvent{
              window::WindowEventType::focusGainedEvent};
        } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
          outEvent.focusLost =
              window::FocusLostEvent{window::WindowEventType::focusLostEvent};
        }
      }
    }

    outWindowEvents.push_back(outEvent);
  }
}
