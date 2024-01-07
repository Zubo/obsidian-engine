#include <iterator>
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
#include <cmath>
#include <thread>
#include <utility>

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
  _context.vulkanRHI.init(extent, _context.window.getWindowBackend(),
                          _context.taskExecutor);

  _context.inputContext.windowEventEmitter.subscribeToWindowResizedEvent(
      [this](std::size_t w, std::size_t h) {
        rhi::WindowExtentRHI newExtent;
        newExtent.width = w;
        newExtent.height = h;
        _context.vulkanRHI.updateExtent(newExtent);
      });

  _context.scene.init(_context.inputContext);
  _context.resourceManager.init(_context.vulkanRHI, _context.project,
                                _context.taskExecutor);
  _context.resourceManager.uploadInitRHIResources();

  _isInitialized = true;
  return _isInitialized;
}

void ObsidianEngine::cleanup() {
  _context.taskExecutor.waitIdle();
  _context.scene.getState().gameObjects.clear();
  _context.inputContext.keyInputEmitter.cleanup();
  _context.inputContext.mouseEventEmitter.cleanup();
  _context.inputContext.windowEventEmitter.cleanup();
  _context.resourceManager.cleanup();
  _context.taskExecutor.shutdown();
  _context.vulkanRHI.cleanup();
}

ObsidianEngineContext& ObsidianEngine::getContext() { return _context; }

ObsidianEngineContext const& ObsidianEngine::getContext() const {
  return _context;
}

bool const ObsidianEngine::isInitialized() const { return _isInitialized; }

void ObsidianEngine::processFrame() {
  scene::SceneState& sceneState = _context.scene.getState();
  ZoneScoped;
  {
    ZoneScopedN("Draw call recursion");
    for (auto& gameObject : sceneState.gameObjects) {
      gameObject.draw(glm::mat4{1.0f});
    }
  }

  rhi::SceneGlobalParams sceneGlobalParams;
  sceneGlobalParams.ambientColor = sceneState.ambientColor;
  sceneGlobalParams.cameraPos = sceneState.camera.pos;
  sceneGlobalParams.cameraRotationRad = sceneState.camera.rotationRad;

  {
    ZoneScopedN("RHI draw");
    _context.vulkanRHI.draw(sceneGlobalParams);
  }
}

void ObsidianEngine::openProject(std::filesystem::path projectPath) {
  _context.taskExecutor.waitIdle();
  _context.scene.getState() = {};
  _context.taskExecutor.shutdown();
  initTaskExecutor();

  _context.resourceManager.cleanup();
  _context.project.open(projectPath);
  _context.resourceManager.init(_context.vulkanRHI, _context.project,
                                _context.taskExecutor);
}

void ObsidianEngine::initTaskExecutor() {
  int const nCores = std::thread::hardware_concurrency();

  _context.taskExecutor.initAndRun(
      {{task::TaskType::rhiTransfer, 4},
       {task::TaskType::rhiUpload, 1},
       {task::TaskType::general,
        static_cast<unsigned>(std::max(nCores - 5, 1))}});
}
