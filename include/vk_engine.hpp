#pragma once

#include <vk_types.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

class DeletionQueue {
public:
  template <typename TFunc> void pushFunction(TFunc &&f) {
    deletionFuncs.emplace_back(std::forward<TFunc>(f));
  }

  void flush() {
    for (auto deletionFuncIter = deletionFuncs.crbegin();
         deletionFuncIter != deletionFuncs.crend(); ++deletionFuncIter)
      (*deletionFuncIter)();

    deletionFuncs.clear();
  }

private:
  std::deque<std::function<void()>> deletionFuncs;
};

class VulkanEngine {
public:
  bool IsInitialized{false};
  int FrameNumber{0};
  VkExtent2D WindowExtent{1000, 800};
  struct SDL_Window *Window{nullptr};

  void init();
  void run();
  void cleanup();
  void draw();

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
  VkPipelineLayout _vkTrianglePipelineLayout;
  VkPipeline _vkTrianglePipeline;
  VkPipeline _vkReverseColorTrianglePipeline;
  int _selectedShader = 0;
  DeletionQueue _deletionQueue;
  VmaAllocator _vmaAllocator;

  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const *filePath, VkShaderModule *outShaderModule);
  void initPipelines();
};

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _vkShaderStageCreateInfo;
  VkPipelineVertexInputStateCreateInfo _vkVertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo _vkInputAssemblyCreateInfo;
  VkViewport _vkViewport;
  VkRect2D _vkScissor;
  VkPipelineRasterizationStateCreateInfo _vkRasterizationCreateInfo;
  VkPipelineColorBlendAttachmentState _vkColorBlendAttachmentState;
  VkPipelineMultisampleStateCreateInfo _vkMultisampleStateCreateInfo;
  VkPipelineLayout _vkTrianglePipelineLayout;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};
