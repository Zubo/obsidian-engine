#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/core/light_types.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/task/task_type.hpp>
#include <obsidian/window/window.hpp>
#include <obsidian/window/window_backend.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <thread>

using namespace obsidian;

bool ObsidianEngine::init(IWindowBackendProvider const& windowBackendProvider,
                          std::filesystem::path projectPath) {
  if (!_context.project.open(projectPath)) {
    OBS_LOG_ERR("Failed to open project at path " + projectPath.string());
    return false;
  }

  initTaskExecutor();

  // create window

  IWindowBackendProvider::CreateWindowParams windowParams;
  windowParams.title = "Obsidian Engine";
  windowParams.posX =
      IWindowBackendProvider::CreateWindowParams::windowCenetered;
  windowParams.posY =
      IWindowBackendProvider::CreateWindowParams::windowCenetered;
  windowParams.width = 1000;
  windowParams.height = 800;

  _context.window.init(windowBackendProvider.createWindow(
                           windowParams, rhi::RHIBackends::vulkan),
                       _context.inputContext);

  auto const vulkanSurfaceProvider = [this](VkInstance vkInstance,
                                            VkSurfaceKHR& outSurface) {
    _context.window.getWindowBackend().provideSurface(_context.vulkanRHI);
  };

  rhi::WindowExtentRHI extent;
  extent.width = windowParams.width;
  extent.height = windowParams.height;

  _context.rhiMainThreadExecutor.initAndRun({{task::TaskType::rhiMain, 1}});

  std::future<void> const vkRhiInitFuture =
      _context.rhiMainThreadExecutor.enqueue(
          task::TaskType::rhiMain, [this, extent]() {
            _context.vulkanRHI.init(extent, _context.window.getWindowBackend());
          });

  vkRhiInitFuture.wait();

  _context.rhiMainThreadExecutor.enqueue(
      task::TaskType::rhiMain, [this, extent]() {
        std::unique_lock l{_renderLoopMutex, std::defer_lock};

        while (!_shutdownRequested.test()) {
          l.lock();

          _renderLoopCondVar.wait(
              l, [&]() { return _readyToRender || _shutdownRequested.test(); });

          if (!_shutdownRequested.test()) {
            ZoneScopedN("RHI draw");

            rhi::SceneGlobalParams sceneGlobalParams;
            scene::SceneState const& sceneState = _context.scene.getState();
            sceneGlobalParams.ambientColor = sceneState.ambientColor;
            sceneGlobalParams.cameraPos = sceneState.camera.pos;
            sceneGlobalParams.cameraRotationRad = sceneState.camera.rotationRad;

            _context.vulkanRHI.draw(sceneGlobalParams);
          }

          _readyToRender = false;

          l.unlock();

          _renderLoopCondVar.notify_all();
        }
      });

  _context.inputContext.windowEventEmitter.subscribeToWindowResizedEvent(
      [this](std::size_t w, std::size_t h) {
        rhi::WindowExtentRHI newExtent;
        newExtent.width = w;
        newExtent.height = h;
        _context.vulkanRHI.updateExtent(newExtent);
      });

  _context.resourceManager.init(_context.vulkanRHI, _context.project,
                                _context.taskExecutor);
  _context.resourceManager.uploadInitRHIResources();
  _context.scene.init(_context.inputContext, _context.vulkanRHI,
                      _context.resourceManager);

  _isInitialized = true;
  return _isInitialized;
}

void ObsidianEngine::cleanup() {
  _context.taskExecutor.waitIdle();
  _context.scene.resetState();
  _context.inputContext.keyInputEmitter.cleanup();
  _context.inputContext.mouseEventEmitter.cleanup();
  _context.inputContext.windowEventEmitter.cleanup();
  _context.resourceManager.cleanup();

  std::future<void> const cleanupFuture =
      _context.rhiMainThreadExecutor.enqueue(
          task::TaskType::rhiMain, [this]() { _context.vulkanRHI.cleanup(); });
  cleanupFuture.wait();

  _context.taskExecutor.shutdown();
  _context.rhiMainThreadExecutor.shutdown();
}

ObsidianEngineContext& ObsidianEngine::getContext() { return _context; }

ObsidianEngineContext const& ObsidianEngine::getContext() const {
  return _context;
}

bool const ObsidianEngine::isInitialized() const { return _isInitialized; }

void ObsidianEngine::prepareRenderData() {
  ZoneScoped;

  if (_shutdownRequested.test()) {
    OBS_LOG_WARN(
        "Prepare render data called after engine shutdown was requested");
    return;
  }

  {
    ZoneScopedN("Draw call recursion");

    for (auto& gameObject : _context.scene.getGameObjects()) {
      gameObject.draw(glm::mat4{1.0f});
    }
  }

  {
    std::scoped_lock l{_renderLoopMutex};
    assert(!_readyToRender);
    _readyToRender = true;
  }

  _renderLoopCondVar.notify_all();
}

void ObsidianEngine::waitFrameProcessed() {
  std::unique_lock l{_renderLoopMutex};
  _renderLoopCondVar.wait(l, [this]() { return !_readyToRender; });
}

void ObsidianEngine::requestShutdown() {
  if (_shutdownRequested.test_and_set()) {
    OBS_LOG_WARN("Shutdown requested multiple times");
    return;
  }

  _renderLoopCondVar.notify_all();
}

void ObsidianEngine::openProject(std::filesystem::path projectPath) {
  _context.taskExecutor.waitIdle();
  _context.scene.resetState();
  _context.taskExecutor.shutdown();
  initTaskExecutor();

  _context.resourceManager.cleanup();
  _context.project.open(projectPath);
  _context.resourceManager.init(_context.vulkanRHI, _context.project,
                                _context.taskExecutor);
}

void ObsidianEngine::initTaskExecutor() {
  int nCores =
      std::max(static_cast<int>(std::thread::hardware_concurrency()), 2);

  _context.taskExecutor.initAndRun(
      {{task::TaskType::resourceUpload, 1},
       {task::TaskType::general,
        static_cast<unsigned>(std::max(nCores - 6, 2))}});
}
