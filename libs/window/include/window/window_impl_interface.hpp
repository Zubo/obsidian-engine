#pragma once

#include <string>
#include <window/window_events.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

namespace obsidian::window::interface {

class IWindowBackend {
public:
  IWindowBackend() = default;
  IWindowBackend(IWindowBackend const& other) = delete;

  virtual ~IWindowBackend() = default;

  IWindowBackend& operator=(IWindowBackend const& other) = delete;

  virtual void provideVulkanSurface(VkInstance vkInstance,
                                    VkSurfaceKHR& outVkSurface) = 0;

  virtual void pollEvents(std::vector<WindowEvent>& outWindowEvents) const = 0;
};

class IWindowBackendProvider {
public:
  IWindowBackendProvider() = default;
  IWindowBackendProvider(IWindowBackendProvider const& other) = delete;

  virtual ~IWindowBackendProvider() = default;

  IWindowBackendProvider&
  operator=(IWindowBackendProvider const& other) = delete;

  struct CreateWindowParams {
    std::string title;
    std::uint32_t posX;
    std::uint32_t posY;
    std::uint32_t width;
    std::uint32_t height;
  };

  virtual std::unique_ptr<IWindowBackend>
  createWindow(CreateWindowParams const& params) const = 0;
};

} /*namespace obsidian::window::interface*/
