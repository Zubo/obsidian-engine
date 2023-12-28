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
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace obsidian::vk_rhi {

class VulkanRHI : public rhi::RHI {
  static unsigned int const maxNumberOfObjects = 10000;
  static unsigned int const shadowPassAttachmentWidth = 2000;
  static unsigned int const shadowPassAttachmentHeight = 2000;
  static unsigned int const environmentMapResolution = 400;

public:
  bool IsInitialized{false};
  int FrameNumber{0};

  void init(rhi::WindowExtentRHI extent,
            rhi::ISurfaceProviderRHI const& surfaceProvider,
            task::TaskExecutor& taskExecutor) override;

  void initResources(rhi::InitResourcesRHI const& initResources) override;

  void initResources();

  void waitDeviceIdle() const override;

  void cleanup() override;

  void draw(rhi::SceneGlobalParams const& sceneParams) override;

  void updateExtent(rhi::WindowExtentRHI newExtent) override;

  rhi::ResourceRHI& initTextureResource() override;

  void uploadTexture(rhi::ResourceIdRHI id,
                     rhi::UploadTextureRHI uploadTextureInfoRHI) override;

  void releaseTexture(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceRHI& initMeshResource() override;

  void uploadMesh(rhi::ResourceIdRHI id, rhi::UploadMeshRHI meshInfo) override;

  void releaseMesh(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceIdRHI
  initObjectResources(glm::vec3 objPos,
                      rhi::ObjectResourceSpecRHI resourceSpec) override;

  rhi::ResourceRHI& initShaderResource() override;

  void uploadShader(rhi::ResourceIdRHI id,
                    rhi::UploadShaderRHI uploadShader) override;

  void releaseShader(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceRHI& initMaterialResource() override;

  void uploadMaterial(rhi::ResourceIdRHI id,
                      rhi::UploadMaterialRHI uploadMaterial) override;

  void releaseMaterial(rhi::ResourceIdRHI resourceIdRHI) override;

  void destroyUnreferencedResources();

  void submitDrawCall(rhi::DrawCall const& drawCall) override;

  void submitLight(rhi::LightSubmitParams const& lightSubmitParams) override;

  VkInstance getInstance() const;

  void setSurface(VkSurfaceKHR surface);

private:
  task::TaskExecutor* _taskExecutor = nullptr;

  // Instance
  VkInstance _vkInstance;
  VkPhysicalDevice _vkPhysicalDevice;
  VkDevice _vkDevice;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;

  // Draw
  VkSurfaceKHR _vkSurface;
  VkPhysicalDeviceProperties _vkPhysicalDeviceProperties;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::uint32_t _graphicsQueueFamilyIndex;
  std::uint32_t _transferQueueFamilyIndex;
  std::unordered_map<std::uint32_t, VkQueue> _gpuQueues;
  mutable std::unordered_map<std::uint32_t, std::mutex> _gpuQueueMutexes;
  std::vector<VKDrawCall> _drawCallQueue;
  std::vector<VKDrawCall> _ssaoDrawCallQueue;
  std::vector<VKDrawCall> _transparentDrawCallQueue;
  std::vector<rhi::DirectionalLight> _submittedDirectionalLights;
  std::vector<rhi::Spotlight> _submittedSpotlights;
  std::array<FrameData, frameOverlap> _frameDataArray = {};
  vkb::Swapchain _vkbSwapchain = {};
  std::vector<VkImageView> _swapchainImageViews;
  std::uint32_t _frameNumber = 0;
  bool _skipFrame = false;
  PFN_vkCmdSetVertexInputEXT _vkCmdSetVertexInput;
  float _maxSamplerAnisotropy;

  // Default pass
  RenderPass _mainRenderPassReuseDepth;
  RenderPass _mainRenderPassNoDepthReuse;
  std::vector<std::array<Framebuffer, frameOverlap>> _vkSwapchainFramebuffers;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipelineLayout _vkLitMeshPipelineLayout;
  AllocatedBuffer _mainRenderPassDataBuffer;

  // Depth pass
  RenderPass _depthRenderPass;
  VkPipelineLayout _vkDepthPipelineLayout;
  VkPipeline _vkDepthPrepassPipeline;
  VkDescriptorSetLayout _vkDepthPassDescriptorSetLayout;
  VkDescriptorSet _depthPrepassDescriptorSet;
  rhi::ResourceIdRHI _depthPassShaderId;
  AllocatedImage _depthPassResultShaderReadImage;
  VkImageView _depthPassResultShaderReadImageView;

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
  VkDescriptorSetLayout _objectDescriptorSetLayout;
  VkDescriptorSetLayout _vkLitTexturedMaterialDescriptorSetLayout;
  VkDescriptorSetLayout _vkUnlitTexturedMaterialDescriptorSetLayout;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _lightDataBuffer;
  VkDescriptorSet _vkGlobalDescriptorSet;
  VkDescriptorSet _emptyDescriptorSet;
  VkSampler _vkLinearRepeatSampler;
  VkSampler _vkDepthSampler;

  // Resources
  std::atomic<rhi::ResourceIdRHI> _nextResourceId = 0;
  std::unordered_map<rhi::ResourceIdRHI, Texture> _textures;
  std::unordered_map<rhi::ResourceIdRHI, Mesh> _meshes;
  std::unordered_map<rhi::ResourceIdRHI, Shader> _shaderModules;
  std::unordered_map<core::MaterialType, PipelineBuilder> _pipelineBuilders;
  std::unordered_map<rhi::ResourceIdRHI, VkMaterial> _materials;
  std::unordered_map<rhi::ResourceIdRHI, VkDescriptorSet> _objectDescriptorSets;
  std::vector<EnvironmentMap> _environmentMaps;
  rhi::ResourceIdRHI _emptyFragShaderId;
  AllocatedBuffer _postProcessingQuadBuffer;
  std::optional<rhi::WindowExtentRHI> _pendingExtentUpdate = std::nullopt;

  // Timer
  AllocatedBuffer _timerStagingBuffer;
  AllocatedBuffer _timerBuffer;
  using Clock = std::chrono::high_resolution_clock;
  std::chrono::time_point<Clock> _engineInitTimePoint;

  // Environment Mapping
  AllocatedBuffer _envMapRenderPassDataBuffer;

  void initVulkan(rhi::ISurfaceProviderRHI const& surfaceProvider);
  void initSwapchain(rhi::WindowExtentRHI const& extent);
  void initCommands();
  void initMainRenderPasses();
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
  void initMainRenderPassDataBuffer();
  void initLightDataBuffer();
  void initDepthSampler();
  void initDescriptors();
  void initDepthPrepassDescriptors();
  void initShadowPassDescriptors();
  void initSsaoDescriptors();
  void initSsaoSamplesAndNoise();
  void initPostProcessingSampler();
  void initSsaoPostProcessingDescriptors();
  void initPostProcessingQuad();
  void initEnvMapRenderPassDataBuffer();
  void initImmediateSubmitContext(ImmediateSubmitContext& context,
                                  std::uint32_t queueInd);
  void initTimer();
  void immediateSubmit(std::uint32_t queueInd,
                       std::function<void(VkCommandBuffer cmd)>&& function);
  void uploadMesh(Mesh& mesh);
  void applyPendingExtentUpdate();
  void updateTimerBuffer(VkCommandBuffer cmd);
  ImmediateSubmitContext&
  getImmediateCtxForCurrentThread(std::uint32_t queueIdx);
  void destroyImmediateCtxForCurrentThread();
  EnvironmentMap& createEnvironmentMap(glm::vec3 envMapPos);

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
                         GPUCameraData const& cameraData,
                         std::vector<std::uint32_t> const& dynamicOffsets,
                         VkDescriptorSet drawPassDescriptorSet,
                         std::optional<VkViewport> dynamicViewport,
                         std::optional<VkRect2D> dynamicScissor,
                         bool reusesDepth = true);
  void drawNoMaterials(VkCommandBuffer, VKDrawCall* first, int count,
                       GPUCameraData const& cameraData, VkPipeline pipeline,
                       VkPipelineLayout pipelineLayout,
                       std::vector<std::uint32_t> const& dynamicOffsets,
                       VkDescriptorSet passDescriptorSet,
                       VertexInputSpec vertexInputSpec,
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
               VkMemoryPropertyFlags preferredFlags = 0,
               VkMemoryPropertyFlags requiredFlags = 0,
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

  void depthPrepass(struct DrawPassParams const& params);
  void ssaoPass(struct DrawPassParams const& params);
  void drawSsaoPostProcessing(struct DrawPassParams const& params);
  void shadowPasses(struct DrawPassParams const& params);
  void colorPass(struct DrawPassParams const& params, glm::vec3 ambientColor,
                 VkFramebuffer targetFramebuffer, VkExtent2D extent);
  void environmentMapPasses(struct DrawPassParams const& params);
  void present(VkSemaphore renderSemaphore, std::uint32_t swapchainImageIndex);
};

} /*namespace obsidian::vk_rhi*/
