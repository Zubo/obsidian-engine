#pragma once

#include <obsidian/rhi/rhi.hpp>
#include <obsidian/window/window_events.hpp>

#include <memory>
#include <string>
#include <vector>

namespace obsidian::window::interface {

class IWindowBackend : public rhi::ISurfaceProviderRHI {
public:
  IWindowBackend() = default;
  IWindowBackend(IWindowBackend const& other) = delete;

  virtual ~IWindowBackend() = default;

  IWindowBackend& operator=(IWindowBackend const& other) = delete;

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
    static constexpr std::uint32_t windowCenetered = ~0;

    std::string title;
    std::uint32_t posX;
    std::uint32_t posY;
    std::uint32_t width;
    std::uint32_t height;
  };

  virtual std::unique_ptr<IWindowBackend>
  createWindow(CreateWindowParams const& params,
               rhi::RHIBackends backend) const = 0;
};

} /*namespace obsidian::window::interface*/
