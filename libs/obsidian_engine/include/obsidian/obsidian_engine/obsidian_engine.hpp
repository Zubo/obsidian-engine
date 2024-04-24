#pragma once

#include <obsidian/input/input_context.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/window/window.hpp>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>

namespace obsidian::window::interface {

class IWindowBackendProvider;

} /*namespace obsidian::window::interface*/

namespace obsidian {

struct ObsidianEngineContext {
  ObsidianEngineContext() = default;
  ObsidianEngineContext(ObsidianEngineContext const& other) = delete;

  task::TaskExecutor taskExecutor;
  task::TaskExecutor rhiMainThreadExecutor;
  input::InputContext inputContext;
  window::Window window;
  vk_rhi::VulkanRHI vulkanRHI;
  project::Project project;
  runtime_resource::RuntimeResourceManager resourceManager;
  scene::Scene scene;
};

class ObsidianEngine {
  using IWindowBackendProvider = window::interface::IWindowBackendProvider;

public:
  ObsidianEngine() = default;
  ObsidianEngine(ObsidianEngine const& other) = delete;

  bool init(IWindowBackendProvider const& windowBackendProvider,
            std::filesystem::path projectPath);
  void cleanup();
  void prepareRenderData();
  void waitFrameProcessed();
  void requestShutdown();
  ObsidianEngineContext& getContext();
  ObsidianEngineContext const& getContext() const;
  bool const isInitialized() const;
  void openProject(std::filesystem::path projectPath);

private:
  void initTaskExecutor();

  ObsidianEngineContext _context;
  bool _isInitialized = false;
  bool _readyToRender = false;
  std::atomic_flag _shutdownRequested;
  std::condition_variable _renderLoopCondVar;
  std::mutex _renderLoopMutex;
};

} /*namespace obsidian*/
