#include <obsidian_engine/obsidian_engine.hpp>
#include <scene/scene.hpp>
#include <vk_rhi/vk_rhi_input.hpp>
#include <window/window.hpp>
#include <window/window_impl_interface.hpp>

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

  _context.window.init(windowBackendProvider.createWindow(windowParams),
                       _context.inputContext);

  auto const vulkanSurfaceProvider = [this](VkInstance vkInstance,
                                            VkSurfaceKHR& outSurface) {
    _context.window.getWindowBackend().provideVulkanSurface(vkInstance,
                                                            outSurface);
  };

  VkExtent2D extent;
  extent.width = windowParams.width;
  extent.height = windowParams.height;
  _context.vulkanRHI.init(extent, vulkanSurfaceProvider, _context.inputContext);

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

  vk_rhi::SceneGlobalParams sceneGlobalParams;
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
