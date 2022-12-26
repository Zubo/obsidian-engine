#pragma once

#include <vk_types.hpp>

#include <cstdint>

class VulkanEngine {
public:
  bool IsInitialized{false};
  int FrameNumber{0};
  VkExtent2D WindowExtent{1000, 800};
  struct SDL_Window *Window{nullptr};

private:
  VkInstance _vkInstance;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;
  VkSurfaceKHR _vkSurface;
  VkPhysicalDevice _vkPhysicalDevice;
  VkDevice _vkDevice;
  VkSwapchainKHR _vkSwapchain;
  VkFormat _vkSwapchainImageFormat;
  std::vector<VkImageView> _vkSwapchainImageViews;
  std::vector<VkImage> _vkSwapchainImages;
  VkQueue _vkGraphicsQueue;
  std::uint32_t _graphicsQueueFamilyIndex;
  VkCommandPool _vkCommandPool;
  VkCommandBuffer _vkCommandBufferMain;

public:
  void init();
  void run();
  void cleanup();
  void draw();

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
};
