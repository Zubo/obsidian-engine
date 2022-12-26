#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <cstdint>
#include <iostream>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      std::cout << "Detected Vulkan error: " << err << std::endl;              \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

void VulkanEngine::init() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WindowFlags windowFlags{static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN)};
  Window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, WindowExtent.width,
                            WindowExtent.height, windowFlags);

  initVulkan();

  initSwapchain();

  initCommands();

  IsInitialized = true;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool shouldQuit = false;

  while (!shouldQuit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        shouldQuit = true;
    }

    draw();
  }
}

void VulkanEngine::cleanup() {
  if (IsInitialized) {
    vkDestroyCommandPool(_vkDevice, _vkCommandPool, nullptr);

    vkDestroySwapchainKHR(_vkDevice, _vkSwapchain, nullptr);

    for (int i = 0; i < _vkSwapchainImageViews.size(); ++i) {
      vkDestroyImageView(_vkDevice, _vkSwapchainImageViews[i], nullptr);
    }

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);
    vkDestroyInstance(_vkInstance, nullptr);
    SDL_DestroyWindow(Window);

    IsInitialized = false;
  }
}

void VulkanEngine::draw() {}

void VulkanEngine::initVulkan() {
  vkb::InstanceBuilder builder;
  auto const builderReturn = builder.set_app_name("VKGuide tutorial")
                                 .request_validation_layers(true)
                                 .require_api_version(1, 1, 0)
                                 .use_default_debug_messenger()
                                 .build();

  vkb::Instance vkbInstance = builderReturn.value();

  _vkInstance = vkbInstance.instance;
  _vkDebugMessenger = vkbInstance.debug_messenger;

  SDL_Vulkan_CreateSurface(Window, _vkInstance, &_vkSurface);
  vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector.set_minimum_version(1, 1)
                                              .set_surface(_vkSurface)
                                              .select()
                                              .value();

  vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
  vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

  _vkDevice = vkbDevice.device;
  _vkPhysicalDevice = vkbPhysicalDevice.physical_device;

  _vkGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initSwapchain() {
  vkb::SwapchainBuilder swapchainBuilder{_vkPhysicalDevice, _vkDevice,
                                         _vkSurface};

  vkb::Swapchain vkbSwapchain =
      swapchainBuilder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(WindowExtent.width, WindowExtent.height)
          .build()
          .value();
  _vkSwapchain = vkbSwapchain.swapchain;
  _vkSwapchainImages = vkbSwapchain.get_images().value();
  _vkSwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::initCommands() {
  VkCommandPoolCreateInfo vkCommandPoolCreateInfo =
      vkinit::commandPoolCreateInfo(
          _graphicsQueueFamilyIndex,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VK_CHECK(vkCreateCommandPool(_vkDevice, &vkCommandPoolCreateInfo, nullptr,
                               &_vkCommandPool));

  constexpr std::uint32_t commandPoolCount = 1;
  VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo =
      vkinit::commandBufferAllocateInfo(_vkCommandPool, commandPoolCount,
                                        VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  VK_CHECK(vkAllocateCommandBuffers(_vkDevice, &vkCommandBufferAllocateInfo,
                                    &_vkCommandBufferMain));
}
