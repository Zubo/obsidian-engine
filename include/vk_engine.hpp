#pragma once

#include <string_view>
#include <vk_deletion_queue.hpp>
#include <vk_mesh.hpp>
#include <vk_pipeline_builder.hpp>
#include <vk_types.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct SDL_KeyboardEvent;
struct SDL_MouseMotionEvent;
struct SDL_Window;

class VulkanEngine {
  static unsigned int const frameOverlap = 2;
  static unsigned int const maxNumberOfObjects = 10000;

public:
  bool IsInitialized{false};
  int FrameNumber{0};
  VkExtent2D WindowExtent{1920, 1080};
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
  DeletionQueue _deletionQueue;
  VmaAllocator _vmaAllocator;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::vector<RenderObject> _renderObjects;
  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;
  std::unordered_map<std::string, Texture> _loadedTextures;
  VkDescriptorSetLayout _vkGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkObjectDataDescriptorSetLayout;
  VkDescriptorPool _vkDescriptorPool;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  VkDescriptorSet _globalDescriptorSet;
  ImmediateSubmitContext _immediateSubmitContext;
  glm::vec3 _cameraPos = {0.f, 10.f, 6.f};
  glm::vec2 _cameraRotationRad = {0.f, 0.f};
  VkSampler _vkSampler;

  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const* filePath, VkShaderModule* outShaderModule);
  void initPipelines();
  void initScene();
  void loadTexture(std::string_view textureName,
                   std::filesystem::path const& texturePath);
  void loadTextures();
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
               VmaAllocationCreateFlags allocationCreateFlags,
               VmaAllocationInfo* outAllocationInfo = nullptr) const;
  void initDescriptors();
  std::size_t getPaddedBufferSize(std::size_t originalSize) const;
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
  bool loadImage(char const* filePath, AllocatedImage& outAllocatedImage);
  void handleKeyboardInput(SDL_KeyboardEvent const& e);
  void handleMoseInput(SDL_MouseMotionEvent const& e);
};
