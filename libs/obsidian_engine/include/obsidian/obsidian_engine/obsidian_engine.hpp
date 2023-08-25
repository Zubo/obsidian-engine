#pragma once

#include <obsidian/input/input_context.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window.hpp>

#include <filesystem>

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
  runtime_resource::RuntimeResourceManager resourceManager;
  project::Project project;
};

class ObsidianEngine {
  using IWindowBackendProvider = window::interface::IWindowBackendProvider;

public:
  ObsidianEngine() = default;
  ObsidianEngine(ObsidianEngine const& other) = delete;

  void init(IWindowBackendProvider const& windowBackendProvider,
            std::filesystem::path rootPath);
  void cleanup();
  void processFrame();
  ObsidianEngineContext& getContext();
  ObsidianEngineContext const& getContext() const;

private:
  ObsidianEngineContext _context;
};

} /*namespace obsidian*/
