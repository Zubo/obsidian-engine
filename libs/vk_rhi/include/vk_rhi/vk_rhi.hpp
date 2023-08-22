#pragma once

#include <vk_rhi/vk_deletion_queue.hpp>
#include <vk_rhi/vk_descriptors.hpp>
#include <vk_rhi/vk_mesh.hpp>
#include <vk_rhi/vk_pipeline_builder.hpp>
#include <vk_rhi/vk_rhi_input.hpp>
#include <vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace obsidian::input {

struct InputContext;

}

namespace obsidian::vk_rhi {

struct SceneGlobalParams;

class VulkanRHI {
  static unsigned int const frameOverlap = 2;
  static unsigned int const maxNumberOfObjects = 10000;
  static unsigned int const shadowPassAttachmentWidth = 2000;
  static unsigned int const shadowPassAttachmentHeight = 2000;

public:
  using SurfaceProvider = std::function<void(VkInstance, VkSurfaceKHR&)>;

  bool IsInitialized{false};
  int FrameNumber{0};

  void init(VkExtent2D extent, SurfaceProvider surfaceProvider,
            input::InputContext& inputContext);
  void cleanup();

  void draw(SceneGlobalParams const& sceneParams);
  void updateExtent(VkExtent2D newExtent);
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
  VkInstance _vkInstance;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;
  VkSurfaceKHR _vkSurface;
  VkPhysicalDevice _vkPhysicalDevice;
  VkPhysicalDeviceProperties _vkPhysicalDeviceProperties;
  VkDevice _vkDevice;
  VkSwapchainKHR _vkSwapchain = VK_NULL_HANDLE;
  VkFormat _vkSwapchainImageFormat;
  std::vector<FramebufferImageViews> _vkFramebufferImageViews;
  std::vector<VkImage> _vkSwapchainImages;
  AllocatedImage _depthImage;
  VkQueue _vkGraphicsQueue;
  std::uint32_t _graphicsQueueFamilyIndex;
  VkRenderPass _vkDefaultRenderPass;
  VkRenderPass _vkShadowRenderPass;
  std::vector<VkFramebuffer> _vkFramebuffers;
  std::array<FrameData, 2> _frameDataArray;
  std::uint32_t _frameNumber = 0;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipelineLayout _vkLitMeshPipelineLayout;
  VkPipelineLayout _vkShadowPassPipelineLayout;
  VkPipeline _vkMeshPipeline;
  VkPipeline _vkLitMeshPipeline;
  VkPipeline _vkShadowPassPipeline;
  DeletionQueue _deletionQueue;
  DeletionQueue _swapchainDeletionQueue;
  VmaAllocator _vmaAllocator;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::vector<RenderObject> _renderObjects;
  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;
  std::unordered_map<std::string, Texture> _loadedTextures;
  DescriptorLayoutCache _descriptorLayoutCache;
  DescriptorAllocator _descriptorAllocator;
  VkDescriptorSetLayout _vkGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkObjectDataDescriptorSetLayout;
  VkDescriptorSetLayout _vkShadowPassGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkLitMeshrenderPassDescriptorSetLayout;
  VkDescriptorSetLayout _vkEmptyDescriptorSetLayout;
  VkDescriptorSetLayout _vkTexturedMaterialDescriptorSetLayout;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _shadowPassCameraBuffer;
  VkDescriptorSet _vkGlobalDescriptorSet;
  VkDescriptorSet _vkShadowPassGlobalDescriptorSet;
  VkDescriptorSet _emptyDescriptorSet;
  ImmediateSubmitContext _immediateSubmitContext;
  VkSampler _vkSampler;
  VkExtent2D _windowExtent;
  bool _skipFrame = false;

  void initVulkan(SurfaceProvider surfaceProvider);
  void initSwapchain();
  void initCommands();
  void initDefaultRenderPass();
  void initShadowRenderPass();
  void initFramebuffers();
  void initShadowPassFramebuffers();
  void initSyncStructures();
  bool loadShaderModule(char const* filePath, VkShaderModule* outShaderModule);
  void initDefaultPipelines();
  void initShadowPassPipeline();
  void initScene();
  void initDescriptors();
  void initShadowPassDescriptors();
  void loadTexture(std::string_view textureName,
                   std::filesystem::path const& texturePath);
  void loadTextures();
  void loadMeshes();
  void uploadMesh(Mesh& mesh);
  FrameData& getCurrentFrameData();
  void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count,
                   SceneGlobalParams const& sceneParams);
  void drawShadowPass(VkCommandBuffer, RenderObject* first, int count,
                      SceneGlobalParams const& sceneGlobalParams);
  Material* createMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                           std::string const& name,
                           std::string const& albedoTexName = "default");
  Material* getMaterial(std::string const& name);
  Mesh* getMesh(std::string const& name);
  AllocatedBuffer
  createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage,
               VmaAllocationCreateFlags allocationCreateFlags,
               VmaAllocationInfo* outAllocationInfo = nullptr) const;
  std::size_t getPaddedBufferSize(std::size_t originalSize) const;
  bool loadImage(char const* filePath, AllocatedImage& outAllocatedImage);
};

} /*namespace obsidian::vk_rhi*/
