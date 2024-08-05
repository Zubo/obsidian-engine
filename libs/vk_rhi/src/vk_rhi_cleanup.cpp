#include <obsidian/renderdoc/renderdoc.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

using namespace obsidian::vk_rhi;

void VulkanRHI::cleanup() {
  if (IsInitialized) {
    renderdoc::deinitRenderdoc();

    waitDeviceIdle();

    _taskExecutor.shutdown();
    while (!_taskExecutor.shutdownComplete())
      ;

    destroyImmediateCtxForCurrentThread();
    cleanupResourceTransferCtxForCurrentThread();

    _swapchainDeletionQueue.flush();
    _deletionQueue.flush();

    vkb::destroy_swapchain(_vkbSwapchain);

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);

    vkDestroyInstance(_vkInstance, nullptr);

    IsInitialized = false;
  }
}
