#include <renderdoc/renderdoc.hpp>
#include <rhi_vk/vk_engine.hpp>

#include <SDL2/SDL.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.hpp>

using namespace obsidian::rhi_vk;

void VulkanEngine::cleanup() {
  if (IsInitialized) {
    renderdoc::deinitRenderdoc();
    vkDeviceWaitIdle(_vkDevice);

    _deletionQueue.flush();

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);

    vkDestroyInstance(_vkInstance, nullptr);
    SDL_DestroyWindow(Window);

    IsInitialized = false;
  }
}
