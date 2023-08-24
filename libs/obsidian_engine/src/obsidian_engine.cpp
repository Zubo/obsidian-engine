#include <obsidian/obsidian_engine/obsidian_engine.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/window/window.hpp>
#include <obsidian/window/window_backend.hpp>

#include <vulkan/vulkan.h>

using namespace obsidian;

void ObsidianEngine::init(IWindowBackendProvider const& windowBackendProvider) {
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
  _context.vulkanRHI.init(extent, _context.window.getWindowBackend());

  _context.inputContext.windowEventEmitter.subscribeToWindowResizedEvent(
      [this](std::size_t w, std::size_t h) {
        rhi::WindowExtentRHI newExtent;
        newExtent.width = w;
        newExtent.height = h;
        _context.vulkanRHI.updateExtent(newExtent);
      });

  _context.scene.init(_context.inputContext);
}

void ObsidianEngine::cleanup() {
  _context.inputContext.keyInputEmitter.cleanup();
  _context.inputContext.mouseMotionEmitter.cleanup();
  _context.inputContext.windowEventEmitter.cleanup();
  _context.vulkanRHI.cleanup();
}

void ObsidianEngine::processFrame() {
  scene::SceneState const& sceneState = _context.scene.getState();

  rhi::SceneGlobalParams sceneGlobalParams;
  sceneGlobalParams.ambientColor = sceneState.ambientColor;
  sceneGlobalParams.sunColor = sceneState.sunColor;
  sceneGlobalParams.sunDirection = sceneState.sunDirection;
  sceneGlobalParams.cameraPos = sceneState.camera.pos;
  sceneGlobalParams.cameraRotationRad = sceneState.camera.rotationRad;

  _context.vulkanRHI.draw(sceneGlobalParams);
}

ObsidianEngineContext& ObsidianEngine::getContext() { return _context; }

ObsidianEngineContext const& ObsidianEngine::getContext() const {
  return _context;
}