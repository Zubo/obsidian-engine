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
  VkRenderPass _vkRenderPass;
  std::vector<VkFramebuffer> _vkFramebuffers;
  VkSemaphore _vkPresentSemaphore, _vkRenderSemaphore;
  VkFence _vkRenderFence;
  std::uint32_t _frameNumber = 0;

public:
  void init();
  void run();
  void cleanup();
  void draw();

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const *filePath, VkShaderModule *outShaderModule);
  void initPipelines();
};
