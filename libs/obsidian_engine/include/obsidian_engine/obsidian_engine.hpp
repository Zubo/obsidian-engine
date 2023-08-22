#pragma once

#include <input/input_context.hpp>
#include <scene/scene.hpp>
#include <vk_rhi/vk_rhi.hpp>
#include <window/window.hpp>

namespace obsidian::window::interface {

class IWindowBackendProvider;

} /*namespace obsidian::window::interface*/

namespace obsidian {

struct ObsidianEngineContext {
  ObsidianEngineContext() = default;
  ObsidianEngineContext(ObsidianEngineContext const& other) = delete;

  vk_rhi::VulkanRHI vulkanRHI;
  scene::Scene scene;
  input::InputContext inputContext;
  window::Window window;
};

class ObsidianEngine {
  using IWindowBackendProvider = window::interface::IWindowBackendProvider;

public:
  ObsidianEngine() = default;
  ObsidianEngine(ObsidianEngine const& other) = delete;

  void init(IWindowBackendProvider const& windowBackendProvider);
  void cleanup();
  void processFrame();
  ObsidianEngineContext& getContext();
  ObsidianEngineContext const& getContext() const;

private:
  ObsidianEngineContext _context;
};

} /*namespace obsidian*/
