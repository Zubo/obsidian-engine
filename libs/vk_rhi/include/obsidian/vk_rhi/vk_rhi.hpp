#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/vk_rhi/vk_deletion_queue.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace obsidian::vk_rhi {

class VulkanRHI : public rhi::RHI {
  static unsigned int const frameOverlap = 2;
  static unsigned int const maxNumberOfObjects = 10000;
  static unsigned int const shadowPassAttachmentWidth = 2000;
  static unsigned int const shadowPassAttachmentHeight = 2000;

public:
  bool IsInitialized{false};
  int FrameNumber{0};

  void init(rhi::WindowExtentRHI extent,
            rhi::ISurfaceProviderRHI const& surfaceProvider) override;

  void initResources(rhi::InitResourcesRHI const& initResources) override;

  void initResources();

  void waitDeviceIdle() const override;

  void cleanup() override;

  void draw(rhi::SceneGlobalParams const& sceneParams) override;

  void updateExtent(rhi::WindowExtentRHI newExtent) override;

  rhi::ResourceIdRHI
  uploadTexture(rhi::UploadTextureRHI const& uploadTextureInfoRHI) override;

  void unloadTexture(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceIdRHI uploadMesh(rhi::UploadMeshRHI const& meshInfo) override;

  void unloadMesh(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceIdRHI
  uploadShader(rhi::UploadShaderRHI const& uploadShader) override;

  void unloadShader(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceIdRHI
  uploadMaterial(rhi::UploadMaterialRHI const& uploadMaterial) override;

  void unloadMaterial(rhi::ResourceIdRHI resourceIdRHI) override;

  void submitDrawCall(rhi::DrawCall const& drawCall) override;

  void submitLight(rhi::LightSubmitParams const& lightSubmitParams) override;

  VkInstance getInstance() const;

  void setSurface(VkSurfaceKHR surface);

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
  AllocatedImage _depthBufferAttachmentImage;
  VkQueue _vkGraphicsQueue;
  std::uint32_t _graphicsQueueFamilyIndex;
  VkRenderPass _vkDefaultRenderPass;
  VkRenderPass _vkDepthRenderPass;
  VkRenderPass _vkSsaoRenderPass;
  std::vector<VkFramebuffer> _vkFramebuffers;
  std::array<FrameData, frameOverlap> _frameDataArray;
  std::uint32_t _frameNumber = 0;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipelineLayout _vkLitMeshPipelineLayout;
  VkPipelineLayout _vkDepthPipelineLayout;
  VkPipelineLayout _vkSsaoPipelineLayout;
  VkPipeline _vkShadowPassPipeline;
  VkPipeline _vkDepthPrepassPipeline;
  VkPipeline _vkSsaoPipeline;
  DeletionQueue _deletionQueue;
  DeletionQueue _swapchainDeletionQueue;
  VmaAllocator _vmaAllocator;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  DescriptorLayoutCache _descriptorLayoutCache;
  DescriptorAllocator _descriptorAllocator;
  VkDescriptorSetLayout _vkGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkObjectDataDescriptorSetLayout;
  VkDescriptorSetLayout _vkDepthPassGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkLitMeshRenderPassDescriptorSetLayout;
  VkDescriptorSetLayout _vkEmptyDescriptorSetLayout;
  VkDescriptorSetLayout _vkTexturedMaterialDescriptorSetLayout;
  VkDescriptorSetLayout _vkSsaoDescriptorSetLayout;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _shadowPassCameraBuffer;
  AllocatedBuffer _lightDataBuffer;
  AllocatedBuffer _ssaoSamplesBuffer;
  rhi::ResourceIdRHI _ssaoNoiseTextureID;
  VkDescriptorSet _vkGlobalDescriptorSet;
  VkDescriptorSet _vkShadowPassGlobalDescriptorSet;
  VkDescriptorSet _emptyDescriptorSet;
  VkDescriptorSet _depthPrepassGlobalDescriptorSet;
  ImmediateSubmitContext _immediateSubmitContext;
  VkSampler _vkAlbedoTextureSampler;
  VkSampler _vkDepthSampler;
  VkExtent2D _windowExtent;
  VkSampler _ssaoNoiseSampler;
  bool _skipFrame = false;
  rhi::ResourceIdRHI _nextResourceId = 0;
  std::unordered_map<rhi::ResourceIdRHI, Texture> _textures;
  std::unordered_map<rhi::ResourceIdRHI, Mesh> _meshes;
  std::unordered_map<rhi::ResourceIdRHI, VkShaderModule> _shaderModules;
  std::unordered_map<core::MaterialType, PipelineBuilder> _pipelineBuilders;
  std::unordered_map<rhi::ResourceIdRHI, Material> _materials;
  rhi::ResourceIdRHI _depthPassShaderId;
  rhi::ResourceIdRHI _ssaoShaderId;
  rhi::ResourceIdRHI _emptyFragShaderId;
  std::vector<VKDrawCall> _drawCallQueue;
  std::vector<rhi::DirectionalLight> _submittedDirectionalLights;
  std::vector<rhi::Spotlight> _submittedSpotlights;

  void initVulkan(rhi::ISurfaceProviderRHI const& surfaceProvider);
  void initSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
  void initCommands();
  void initDefaultRenderPass();
  void initDepthRenderPass();
  void initSsaoRenderPass();
  void initFramebuffers();
  void initDepthPrepassFramebuffers();
  void initShadowPassFramebuffers();
  void initSsaoFramebuffers();
  void initSyncStructures();
  void initDefaultPipelineAndLayouts();
  void initDepthPassPipelineLayout();
  void initShadowPassPipeline();
  void initSsaoPipeline();
  void initDepthPrepassPipeline();
  void initScene();
  void initDescriptors();
  void initDepthPrepassDescriptors();
  void initShadowPassDescriptors();
  void initSsaoDescriptors();
  void initSSAOSamplesAndNoise();
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
  void uploadMesh(Mesh& mesh);
  FrameData& getCurrentFrameData();
  void drawObjects(VkCommandBuffer cmd, VKDrawCall* first, int count,
                   rhi::SceneGlobalParams const& sceneParams);
  void drawDepthPass(VkCommandBuffer, VKDrawCall* first, int count,
                     VkPipeline pipeline, VkDescriptorSet globalDescriptorSet,
                     AllocatedBuffer const& cameraBuffer,
                     std::size_t cameraDataInd, GPUCameraData const& cameraData,
                     std::optional<VkViewport> dynamicViewport = std::nullopt,
                     std::optional<VkRect2D> dynamicScissor = std::nullopt);
  Mesh* getMesh(std::string const& name);
  AllocatedBuffer
  createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage,
               VmaAllocationCreateFlags allocationCreateFlags,
               VmaAllocationInfo* outAllocationInfo = nullptr) const;
  std::size_t getPaddedBufferSize(std::size_t originalSize) const;

  rhi::ResourceIdRHI consumeNewResourceId();
  int getNextAvailableShadowMapIndex();
  void submitLight(rhi::DirectionalLightParams const& directionalLight);
  void submitLight(rhi::SpotlightParams const& spotlight);
  std::vector<ShadowPassParams> getSubmittedShadowPassParams() const;
  GPUCameraData
  getSceneCameraData(rhi::SceneGlobalParams const& sceneParams) const;
  GPULightData getGPULightData() const;
  void createDepthImage(AllocatedImage& outImage,
                        VkImageUsageFlags flags) const;
};

} /*namespace obsidian::vk_rhi*/
