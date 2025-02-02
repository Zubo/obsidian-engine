#pragma once

#include <obsidian/core/material.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/rhi/submit_types_rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_debug.hpp>
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
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
            rhi::ISurfaceProviderRHI const& surfaceProvider) override;

  void initResources(rhi::InitResourcesRHI const& initResources) override;

  void waitDeviceIdle() const override;

  void cleanup() override;

  void clearFrameData();

  void draw(rhi::SceneGlobalParams const& sceneParams) override;

  void updateExtent(rhi::WindowExtentRHI newExtent) override;

  rhi::ResourceRHI& initTextureResource() override;

  rhi::ResourceTransferRHI
  uploadTexture(rhi::ResourceIdRHI id,
                rhi::UploadTextureRHI uploadTextureInfoRHI) override;

  void releaseTexture(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceRHI& initMeshResource() override;

  rhi::ResourceTransferRHI uploadMesh(rhi::ResourceIdRHI id,
                                      rhi::UploadMeshRHI meshInfo) override;

  void releaseMesh(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceRHI& initShaderResource() override;

  rhi::ResourceTransferRHI
  uploadShader(rhi::ResourceIdRHI id,
               rhi::UploadShaderRHI uploadShader) override;

  void releaseShader(rhi::ResourceIdRHI resourceIdRHI) override;

  rhi::ResourceRHI& initMaterialResource() override;

  rhi::ResourceTransferRHI
  uploadMaterial(rhi::ResourceIdRHI id,
                 rhi::UploadMaterialRHI uploadMaterial) override;

  void releaseMaterial(rhi::ResourceIdRHI resourceIdRHI) override;

  void destroyUnusedResources(bool forceDestroy);

  void submitDrawCall(rhi::DrawCall const& drawCall) override;

  void submitLight(rhi::LightSubmitParams const& lightSubmitParams) override;

  VkInstance getInstance() const;

  void setSurface(VkSurfaceKHR surface);

  rhi::ResourceIdRHI createEnvironmentMap(glm::vec3 pos, float radius) override;

  void releaseEnvironmentMap(rhi::ResourceIdRHI envMapId) override;

  void updateEnvironmentMap(rhi::ResourceIdRHI envMapId, glm::vec3 pos,
                            float radius) override;

  void applyPendingEnvironmentMapUpdates();

private:
  task::TaskExecutor _taskExecutor;

  // Instance
  VkInstance _vkInstance;
  VkPhysicalDevice _vkPhysicalDevice;
  VkDevice _vkDevice;
  VkDebugUtilsMessengerEXT _vkDebugMessenger;

  // Draw
  VkSurfaceKHR _vkSurface;
  VkPhysicalDeviceProperties _vkPhysicalDeviceProperties;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  std::unordered_set<std::uint32_t> _queueFamilyIndices;
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
  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  std::atomic<std::uint32_t> _frameNumber = 1;
  VkSemaphore _frameNumberSemaphore;
  PFN_vkCmdSetVertexInputEXT _vkCmdSetVertexInput;
  float _maxSamplerAnisotropy;

  // Default pass
  RenderPass _mainRenderPass;
  std::vector<std::array<Framebuffer, frameOverlap>> _vkSwapchainFramebuffers;
  VkPipelineLayout _vkMeshPipelineLayout;
  VkPipelineLayout _vkLitMeshPipelineLayout;
  VkPipelineLayout _vkPbrMeshPipelineLayout;
  AllocatedBuffer _mainRenderPassDataBuffer;

  // Depth pass
  RenderPass _depthRenderPass;
  VkPipelineLayout _vkDepthPipelineLayout;
  VkPipeline _vkDepthPrepassPipeline;
  VkDescriptorSetLayout _vkDepthPassDescriptorSetLayout;
  VkDescriptorSet _depthPrepassDescriptorSet;
  rhi::ResourceIdRHI _depthPassVertexShaderId;
  rhi::ResourceIdRHI _depthPassFragmentShaderId;

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
  rhi::ResourceIdRHI _ssaoVertexShaderId;
  rhi::ResourceIdRHI _ssaoFragmentShaderId;
  std::uint32_t _ssaoResolutionDivider = 2;

  // Post processing
  RenderPass _postProcessingRenderPass;
  VkSampler _postProcessingImageSampler;
  rhi::ResourceIdRHI _postProcessingVertexShaderId;
  rhi::ResourceIdRHI _postProcessingFragmentShaderId;

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
  VkDescriptorSetLayout _vkPbrMaterialDescriptorSetLayout;
  AllocatedBuffer _sceneDataBuffer;
  AllocatedBuffer _cameraBuffer;
  AllocatedBuffer _globalSettingsBuffer;
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
  std::unordered_map<rhi::ResourceIdRHI, EnvironmentMap> _environmentMaps;
  AllocatedBuffer _postProcessingQuadBuffer;
  std::optional<rhi::WindowExtentRHI> _pendingExtentUpdate = std::nullopt;
  std::mutex _pendingExtentUpdateMutex;
  PendingResourcesToDestroy _pendingResourcesToDestroy;
  std::mutex _pendingResourcesToDestroyMutex;

  // Timer
  AllocatedBuffer _timerBuffer;
  using Clock = std::chrono::high_resolution_clock;
  std::chrono::time_point<Clock> _engineInitTimePoint;

  // Environment Mapping
  RenderPass _envMapRenderPass;
  AllocatedBuffer _envMapRenderPassDataBuffer;
  AllocatedBuffer _envMapDataBuffer;
  bool _envMapDescriptorSetPendingUpdate = false;
  VkFormat _envMapFormat = VK_FORMAT_R8G8B8A8_SRGB;

  // MSAA
  VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_4_BIT;

  void initVulkan(rhi::ISurfaceProviderRHI const& surfaceProvider);
  void initFrameNumberSemaphore();
  void initSwapchain(rhi::WindowExtentRHI const& extent);
  void initCommands();
  void initMainRenderPasses();
  void initDepthRenderPass();
  void initSsaoRenderPass();
  void initPostProcessingRenderPass();
  void initSwapchainFramebuffers();
  void initDepthPrepassFramebuffers();
  void initShadowPassFramebuffers();
  void initGlobalSettingsBuffer();
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
  void initEnvMapDataBuffer();
  void initImmediateSubmitContext(ImmediateSubmitContext& context,
                                  std::uint32_t queueInd);
  void initTimer();
  void initEnvMapRenderPassDescriptorSets();
  void immediateSubmit(std::uint32_t queueInd,
                       std::function<void(VkCommandBuffer cmd)>&& function);
  void transferDataToImage(AllocatedBuffer stagingBuffer, VkImage dstImg,
                           ImageTransferInfo const& imageTransferInfo,
                           std::uint32_t currentImgQeueueFamilyIdx,
                           ImageTransferDstState transferDstState);
  void transferDataToBuffer(
      AllocatedBuffer stagingBuffer,
      std::vector<BufferTransferInfo> const& bufferTransferInfos,
      std::uint32_t currentBufferQeueueFamilyIdx,
      BufferTransferOptions bufferTransferOptions);
  void initResourceTransferContext(ResourceTransferContext& ctx);
  void cleanupFinishedTransfersForCurrentThread(bool waitToFinish);
  ResourceTransferContext& getResourceTransferContextForCurrentThread();
  void applyPendingExtentUpdate();
  void updateTimerBuffer(VkCommandBuffer cmd);
  ImmediateSubmitContext&
  getImmediateCtxForCurrentThread(std::uint32_t queueIdx);
  void destroyImmediateCtxForCurrentThread();
  void cleanupResourceTransferCtxForCurrentThread();
  void updateGlobalSettingsBuffer(bool init);

  FrameData& getCurrentFrameData();
  FrameData& getPreviousFrameData();

  template <typename T>
  void uploadBufferData(std::size_t const index, T const& value,
                        AllocatedBuffer const& dstBuffer,
                        std::uint32_t bufferQueueFamilyInd,
                        VkAccessFlags srcAccessMask = VK_ACCESS_NONE) {
    using ValueType = std::decay_t<T>;

    std::size_t const valueSize = getPaddedBufferSize(sizeof(ValueType));

    AllocatedBuffer stagingBuffer = createBuffer(
        valueSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    setDbgResourceName(_vkDevice, (std::uint64_t)stagingBuffer.buffer,
                       VK_OBJECT_TYPE_BUFFER, "Upload buffer staging buffer");

    void* data = nullptr;

    VK_CHECK(vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &data));

    std::memcpy(data, reinterpret_cast<char const*>(&value), sizeof(ValueType));

    vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);
    (void)data;

    VkDeviceSize const offset = index * valueSize;

    vmaFlushAllocation(_vmaAllocator, stagingBuffer.allocation, offset,
                       valueSize);

    BufferTransferInfo bufferTransferInfo = {.srcOffset = 0,
                                             .dstOffset = offset,
                                             .size = valueSize,
                                             .dstBuffer = dstBuffer.buffer};

    BufferTransferOptions const bufferTransferOptions = {
        .dstBufferQueueFamilyIdx = _graphicsQueueFamilyIndex,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstPipelineStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

    transferDataToBuffer(stagingBuffer, {bufferTransferInfo},
                         bufferQueueFamilyInd, bufferTransferOptions);
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
  drawPostProcessing(VkCommandBuffer cmd, glm::mat4x4 const& kernel,
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
  std::vector<ShadowPassParams>
  getSubmittedShadowPassParams(glm::vec3 mainCameraPos) const;
  GPUCameraData
  getSceneCameraData(rhi::SceneGlobalParams const& sceneParams) const;
  GPULightData getGPULightData(glm::vec3 mainCameraPos) const;
  VkExtent2D getSsaoExtent() const;
  VkViewport getSsaoViewport() const;
  VkRect2D getSsaoScissor() const;

  void depthPrepass(struct DrawPassParams const& params);
  void ssaoPass(struct DrawPassParams const& params);
  void ssaoPostProcessingPass(struct DrawPassParams const& params);
  void shadowPasses(struct DrawPassParams const& params);
  void colorPass(struct DrawPassParams const& params, glm::vec3 ambientColor,
                 VkFramebuffer targetFramebuffer, VkExtent2D extent);
  void environmentMapPasses(struct DrawPassParams const& params);
  void present(VkSemaphore renderSemaphore, std::uint32_t swapchainImageIndex);
};

} /*namespace obsidian::vk_rhi*/
