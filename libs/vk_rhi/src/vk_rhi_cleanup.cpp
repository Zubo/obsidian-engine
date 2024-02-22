#include <obsidian/renderdoc/renderdoc.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

using namespace obsidian::vk_rhi;

void VulkanRHI::cleanup() {
  if (IsInitialized) {
    renderdoc::deinitRenderdoc();

    assert(_taskExecutor);
    while (!_taskExecutor->shutdownComplete())
      ;

    waitDeviceIdle();

    _swapchainDeletionQueue.flush();
    _deletionQueue.flush();

    destroyImmediateCtxForCurrentThread();
    destroyResourceTransferContextForCurrentThread();

    vkb::destroy_swapchain(_vkbSwapchain);

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);

    vkDestroyInstance(_vkInstance, nullptr);

    IsInitialized = false;
  }
}
