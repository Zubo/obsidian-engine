#pragma once

#include <vk_mesh.hpp>
#include <vk_types.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Material {
  VkPipeline vkPipeline;
  VkPipelineLayout vkPipelineLayout;
};

struct RenderObject {
  Mesh* mesh;
  Material* material;
  glm::mat4 transformMatrix;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 renderMatrix;
};

constexpr unsigned int frameOverlap = 2;

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewProj;
};

struct GPUSceneData {
  glm::vec4 fogColor;
  glm::vec4 fogDistance;
  glm::vec4 ambientColor;
  glm::vec4 sunlightdirection;
  glm::vec4 sunlightcolor;
};

struct FrameData {
  VkSemaphore vkRenderSemaphore;
  VkSemaphore vkPresentSemaphore;
  VkFence vkRenderFence;
  VkCommandPool vkCommandPool;
  VkCommandBuffer vkCommandBuffer;

  AllocatedBuffer cameraBuffer;
  VkDescriptorSet globalDescriptorSet;
};

class DeletionQueue {
public:
  template <typename TFunc> void pushFunction(TFunc&& f) {
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

struct FramebufferImageViews {
  std::vector<VkImageView> vkImageViews;
};

struct AllocatedImage {
  VkImage vkImage;
  VmaAllocation allocation;
};

class VulkanEngine {
public:
  bool IsInitialized{false};
  int FrameNumber{0};
  VkExtent2D WindowExtent{1000, 800};
  struct SDL_Window* Window{nullptr};

  void init();
  void run();
  void cleanup();

private:
  VkInstance _vkInstance;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;
  VkSurfaceKHR _vkSurface;
  VkPhysicalDevice _vkPhysicalDevice;
  VkPhysicalDeviceProperties _vkPhysicalDeviceProperties;
  VkDevice _vkDevice;
  VkSwapchainKHR _vkSwapchain;
  VkFormat _vkSwapchainImageFormat;
  std::vector<FramebufferImageViews> _vkFramebufferImageViews;
  std::vector<VkImage> _vkSwapchainImages;
  AllocatedImage _depthImage;
  VkQueue _vkGraphicsQueue;
  std::uint32_t _graphicsQueueFamilyIndex;
  VkRenderPass _vkRenderPass;
  std::vector<VkFramebuffer> _vkFramebuffers;
  std::array<FrameData, 2> _frameDataArray;
  std::uint32_t _frameNumber = 0;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipeline _vkMeshPipeline;
  int _selectedShader = 0;
  DeletionQueue _deletionQueue;
  VmaAllocator _vmaAllocator;
  Mesh _triangleMesh;
  Mesh _monkeyMesh;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::vector<RenderObject> _renderObjects;
  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;
  VkDescriptorSetLayout _vkGlobalDescriptorSetLayout;
  VkDescriptorPool _vkDescriptorPool;
  AllocatedBuffer _sceneDataBuffer;

  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const* filePath, VkShaderModule* outShaderModule);
  void initPipelines();
  void initScene();
  void loadMeshes();
  void uploadMesh(Mesh& mesh);
  FrameData& getCurrentFrameData();
  void draw();
  void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);
  Material* createMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                           std::string const& name);
  Material* getMaterial(std::string const& name);
  Mesh* getMesh(std::string const& name);
  AllocatedBuffer
  createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage,
               VmaAllocationCreateFlags allocationCreateFlags) const;
  void initDescriptors();
  std::size_t getPaddedBufferSize(std::size_t originalSize) const;
};

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _vkShaderStageCreateInfo;
  VkPipelineVertexInputStateCreateInfo _vkVertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo _vkInputAssemblyCreateInfo;
  VkPipelineDepthStencilStateCreateInfo _vkDepthStencilStateCreateInfo;
  VkViewport _vkViewport;
  VkRect2D _vkScissor;
  VkPipelineRasterizationStateCreateInfo _vkRasterizationCreateInfo;
  VkPipelineColorBlendAttachmentState _vkColorBlendAttachmentState;
  VkPipelineMultisampleStateCreateInfo _vkMultisampleStateCreateInfo;
  VkPipelineLayout _vkPipelineLayout;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};
