#pragma once

#include <vk_mesh.hpp>
#include <vk_types.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

struct Material {
  VkPipeline vkPipeline;
  VkPipelineLayout vkPipelineLayout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 transformMatrix;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 renderMatrix;
};

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
  struct SDL_Window *Window{nullptr};

  void init();
  void run();
  void cleanup();
  void draw();
  void drawObjects(VkCommandBuffer cmd, RenderObject *first, int count);
  Material *createMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                           std::string const &name);
  Material *getMaterial(std::string const &name);
  Mesh *getMesh(std::string const &name);

private:
  VkInstance _vkInstance;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;
  VkSurfaceKHR _vkSurface;
  VkPhysicalDevice _vkPhysicalDevice;
  VkDevice _vkDevice;
  VkSwapchainKHR _vkSwapchain;
  VkFormat _vkSwapchainImageFormat;
  std::vector<FramebufferImageViews> _vkFramebufferImageViews;
  std::vector<VkImage> _vkSwapchainImages;
  AllocatedImage _depthImage;
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
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipeline _vkTrianglePipeline;
  VkPipeline _vkReverseColorTrianglePipeline;
  int _selectedShader = 0;
  DeletionQueue _deletionQueue;
  VmaAllocator _vmaAllocator;
  VkPipeline _vkMeshPipeline;
  Mesh _triangleMesh;
  Mesh _monkeyMesh;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::vector<RenderObject> _renderObjects;
  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const *filePath, VkShaderModule *outShaderModule);
  void initPipelines();
  void initScene();
  void loadMeshes();
  void uploadMesh(Mesh &mesh);
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
