#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_deletion_queue.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_frame_data.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
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
  // Instance
  VkInstance _vkInstance;
  VkPhysicalDevice _vkPhysicalDevice;
  VkDevice _vkDevice;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;

  // Draw
  VkSurfaceKHR _vkSurface;
  VkPhysicalDeviceProperties _vkPhysicalDeviceProperties;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  VkQueue _vkGraphicsQueue;
  std::uint32_t _graphicsQueueFamilyIndex;
  std::vector<VKDrawCall> _drawCallQueue;
  std::vector<rhi::DirectionalLight> _submittedDirectionalLights;
  std::vector<rhi::Spotlight> _submittedSpotlights;
  std::array<FrameData, frameOverlap> _frameDataArray = {};
  vkb::Swapchain _vkbSwapchain = {};
  std::vector<VkImageView> _swapchainImageViews;
  std::uint32_t _frameNumber = 0;
  bool _skipFrame = false;

  // Default pass
  RenderPass _mainRenderPass;
  std::vector<Framebuffer> _vkSwapchainFramebuffers;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipelineLayout _vkLitMeshPipelineLayout;

  // Depth pass
  RenderPass _depthRenderPass;
  VkPipelineLayout _vkDepthPipelineLayout;
  VkPipeline _vkDepthPrepassPipeline;
  VkDescriptorSetLayout _vkDepthPassDescriptorSetLayout;
  VkDescriptorSet _depthPrepassDescriptorSet;
  rhi::ResourceIdRHI _depthPassShaderId;

  // Shadow pass
  VkPipeline _vkShadowPassPipeline;
  AllocatedBuffer _shadowPassCameraBuffer;
  VkDescriptorSet _vkShadowPassDescriptorSet;

  // Ssao
  RenderPass _ssaoRenderPass;
  VkPipeline _vkSsaoPipeline;
  VkPipelineLayout _vkSsaoPipelineLayout;
  VkFormat _ssaoFormat = VK_FORMAT_R32_SFLOAT;
  VkDescriptorSetLayout _vkSsaoDescriptorSetLayout;
  AllocatedBuffer _ssaoSamplesBuffer;
  rhi::ResourceIdRHI _ssaoNoiseTextureID;
  VkSampler _ssaoNoiseSampler;
  rhi::ResourceIdRHI _ssaoShaderId;

  // Post processing
  RenderPass _postProcessingRenderPass;
  VkSampler _postProcessingImageSampler;
  rhi::ResourceIdRHI _postProcessingShaderId;

  // Ssao post processing
  VkPipelineLayout _vkSsaoPostProcessingPipelineLayout;
  VkPipeline _vkSsaoPostProcessingPipeline;
  VkDescriptorSetLayout _vkSsaoPostProcessingDescriptorSetLayout;

  DeletionQueue _deletionQueue;
  DeletionQueue _swapchainDeletionQueue;
  VmaAllocator _vmaAllocator;
  DescriptorLayoutCache _descriptorLayoutCache;
  DescriptorAllocator _descriptorAllocator;
  DescriptorAllocator _swapchainBoundDescriptorAllocator;
  VkDescriptorSetLayout _vkGlobalDescriptorSetLayout;
  VkDescriptorSetLayout _vkMainRenderPassDescriptorSetLayout;
  VkDescriptorSetLayout _vkEmptyDescriptorSetLayout;
  VkDescriptorSetLayout _vkLitTexturedMaterialDescriptorSetLayout;
  VkDescriptorSetLayout _vkUnlitTexturedMaterialDescriptorSetLayout;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _lightDataBuffer;
  VkDescriptorSet _vkGlobalDescriptorSet;
  VkDescriptorSet _emptyDescriptorSet;
  ImmediateSubmitContext _immediateSubmitContext;
  VkSampler _vkLinearClampToEdgeSampler;
  VkSampler _vkDepthSampler;

  // Resources
  rhi::ResourceIdRHI _nextResourceId = 0;
  std::unordered_map<rhi::ResourceIdRHI, Texture> _textures;
  std::unordered_map<rhi::ResourceIdRHI, Mesh> _meshes;
  std::unordered_map<rhi::ResourceIdRHI, VkShaderModule> _shaderModules;
  std::unordered_map<core::MaterialType, PipelineBuilder> _pipelineBuilders;
  std::unordered_map<rhi::ResourceIdRHI, Material> _materials;
  rhi::ResourceIdRHI _emptyFragShaderId;
  AllocatedBuffer _postProcessingQuadBuffer;
  std::optional<rhi::WindowExtentRHI> _pendingExtentUpdate = std::nullopt;

  void initVulkan(rhi::ISurfaceProviderRHI const& surfaceProvider);
  void initSwapchain(rhi::WindowExtentRHI const& extent);
  void initCommands();
  void initMainRenderPass();
  void initDepthRenderPass();
  void initSsaoRenderPass();
  void initPostProcessingRenderPass();
  void initSwapchainFramebuffers();
  void initDepthPrepassFramebuffers();
  void initShadowPassFramebuffers();
  void initSsaoFramebuffers();
  void initSsaoPostProcessingFramebuffers();
  void initSyncStructures();
  void initMainPipelineAndLayouts();
  void initDepthPassPipelineLayout();
  void initShadowPassPipeline();
  void initSsaoPipeline();
  void initDepthPrepassPipeline();
  void initSsaoPostProcessingPipeline();
  void initDescriptorBuilder();
  void initDefaultSamplers();
  void initDescriptors();
  void initDepthPrepassDescriptors();
  void initShadowPassDescriptors();
  void initSsaoDescriptors();
  void initSsaoSamplesAndNoise();
  void initPostProcessingSampler();
  void initSsaoPostProcessingDescriptors();
  void initPostProcessingQuad();
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
  void uploadMesh(Mesh& mesh);
  void applyPendingExtentUpdate();

  FrameData& getCurrentFrameData();

  template <typename T>
  void uploadBufferData(std::size_t const index, T const& value,
                        AllocatedBuffer const& buffer) {
    using ValueType = std::decay_t<T>;
    void* data = nullptr;

    VK_CHECK(vmaMapMemory(_vmaAllocator, buffer.allocation, &data));

    char* const dstBufferData = reinterpret_cast<char*>(data) +
                                index * getPaddedBufferSize(sizeof(ValueType));

    std::memcpy(dstBufferData, &value, sizeof(ValueType));

    vmaUnmapMemory(_vmaAllocator, buffer.allocation);
  }

  void drawWithMaterials(VkCommandBuffer cmd, VKDrawCall* first, int count,
                         std::vector<std::uint32_t> const& dynamicOffsets,
                         VkDescriptorSet drawPassDescriptorSet,
                         std::optional<VkViewport> dynamicViewport,
                         std::optional<VkRect2D> dynamicScissor);
  void
  drawPassNoMaterials(VkCommandBuffer, VKDrawCall* first, int count,
                      VkPipeline pipeline, VkPipelineLayout pipelineLayout,
                      std::vector<std::uint32_t> const& dynamicOffsets,
                      VkDescriptorSet passDescriptorSet,
                      std::optional<VkViewport> dynamicViewport = std::nullopt,
                      std::optional<VkRect2D> dynamicScissor = std::nullopt);
  void
  drawPostProcessing(VkCommandBuffer cmd, glm::mat3x3 const& kernel,
                     VkFramebuffer frameBuffer,
                     VkDescriptorSet passDescriptorSet,
                     std::optional<VkViewport> dynamicViewport = std::nullopt,
                     std::optional<VkRect2D> dynamicScissor = std::nullopt);
  Mesh* getMesh(std::string const& name);
  AllocatedBuffer
  createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage,
               VmaAllocationCreateFlags allocationCreateFlags,
               VmaAllocationInfo* outAllocationInfo = nullptr) const;
  std::size_t getPaddedBufferSize(std::size_t originalSize) const;

  template <typename MaterialDataT>
  void createAndBindMaterialDataBuffer(MaterialDataT const& materialData,
                                       DescriptorBuilder& builder,
                                       VkDescriptorBufferInfo& bufferInfo);
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
