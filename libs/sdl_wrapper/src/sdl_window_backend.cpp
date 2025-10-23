#include <SDL3/SDL_oldnames.h>
#include <obsidian/core/keycode.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/sdl_wrapper/sdl_backend.hpp>
#include <obsidian/sdl_wrapper/sdl_window_backend.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window_events.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

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

  SDL_Vulkan_LoadLibrary(nullptr);

  Uint32 count;
  char const* const* ext = SDL_Vulkan_GetInstanceExtensions(&count);

  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(_sdlWindowUnique.get(),
                                vulkanRhi->getInstance(), nullptr, &surface)) {
    OBS_LOG_ERR(std::string{"Failed to create surface. Error: "} +
                SDL_GetError());
  }

  vulkanRhi->setSurface(surface);
}

bool SDLWindowBackend::showWindow() const {
  return SDL_ShowWindow(_sdlWindowUnique.get());
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
    case SDL_EVENT_KEY_DOWN:
      if (e.key.windowID == windowID) {
        outEvent.keyDownEvent =
            window::KeyDownEvent{window::WindowEventType::keyDown,
                                 static_cast<core::KeyCode>(e.key.key)};
      }
      break;
    case SDL_EVENT_KEY_UP:
      if (e.key.windowID == windowID) {
        outEvent.keyUpEvent =
            window::KeyUpEvent{window::WindowEventType::keyUp,
                               static_cast<core::KeyCode>(e.key.key)};
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (e.motion.windowID == windowID) {
        outEvent.mouseMotionEvent =
            window::MouseMotionEvent{window::WindowEventType::mouseMotion,
                                     static_cast<std::int32_t>(e.motion.xrel),
                                     static_cast<std::int32_t>(e.motion.yrel)};
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (e.button.windowID == windowID) {
        core::MouseButtonType const button =
            getMouseButtonType(e.button.button);
        outEvent.mouseButtonDownEvent = window::MouseButtonDownEvent{
            window::WindowEventType::mouseButtonDown, button};
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.windowID == windowID) {
        core::MouseButtonType const button =
            getMouseButtonType(e.button.button);
        outEvent.mouseButtonUpEvent = window::MouseButtonUpEvent{
            window::WindowEventType::mouseButtonUp, button};
      }
      break;
    default:
      if (e.type >= SDL_EVENT_WINDOW_SHOWN && e.type <= SDL_EVENT_WINDOW_LAST &&
          e.window.windowID == windowID) {
        if (e.type == SDL_EVENT_WINDOW_RESIZED ||
            e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
          outEvent.windowResized =
              window::WindowResizedEvent{window::WindowEventType::windowResized,
                                         e.window.data1, e.window.data2};
        } else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
          outEvent.shouldQuitEvent =
              window::ShouldQuitEvent{window::WindowEventType::shouldQuit};
        } else if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
          outEvent.focusGained = window::FocusGainedEvent{
              window::WindowEventType::focusGainedEvent};
        } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
          outEvent.focusLost =
              window::FocusLostEvent{window::WindowEventType::focusLostEvent};
        }
      }
    }

    outWindowEvents.push_back(outEvent);
  }
}
