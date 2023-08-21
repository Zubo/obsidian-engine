#include <renderdoc/renderdoc.hpp>
#include <vk_rhi/vk_rhi.hpp>

#include <SDL2/SDL.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.hpp>

using namespace obsidian::vk_rhi;

void VulkanRHI::cleanup() {
  if (IsInitialized) {
    renderdoc::deinitRenderdoc();
    vkDeviceWaitIdle(_vkDevice);

    _swapchainDeletionQueue.flush();
    _deletionQueue.flush();

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);

    vkDestroyInstance(_vkInstance, nullptr);
    SDL_DestroyWindow(Window);

    IsInitialized = false;
  }
}
