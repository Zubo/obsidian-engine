#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/renderdoc/renderdoc.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_debug.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_frame_data.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_pipeline_builder.hpp>
#include <obsidian/vk_rhi/vk_render_pass_builder.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cmath>
#include <cstring>
#include <numeric>
#include <random>

using namespace obsidian::vk_rhi;

void VulkanRHI::init(rhi::WindowExtentRHI extent,
                     rhi::ISurfaceProviderRHI const& surfaceProvider) {
  _taskExecutor.initAndRun({{task::TaskType::rhiTransfer, 4}});

  renderdoc::initRenderdoc();

  initVulkan(surfaceProvider);

  _deletionQueue.pushFunction([this]() {
    for (FrameData& frameData : _frameDataArray) {
      destroyUnusedResources(frameData.pendingResourcesToDestroy);
    }
  });

  initSwapchain(extent);
  initCommands();
  initMainRenderPasses();
  initDepthRenderPass();
  initSsaoRenderPass();
  initPostProcessingRenderPass();
  initPostProcessingQuad();
  initPostProcessingSampler();
  initDepthPrepassFramebuffers();
  initSwapchainFramebuffers();
  initShadowPassFramebuffers();
  initSsaoFramebuffers();
  initSsaoPostProcessingFramebuffers();
  initSyncStructures();
  initDescriptorBuilder();
  initDefaultSamplers();
  initMainRenderPassDataBuffer();
  initLightDataBuffer();
  initDepthSampler();
  initEnvMapDataBuffer();
  initGlobalSettingsBuffer();
  initDescriptors();
  initDepthPrepassDescriptors();
  initShadowPassDescriptors();
  initSsaoSamplesAndNoise();
  initSsaoDescriptors();
  initSsaoPostProcessingDescriptors();
  initMainPipelineAndLayouts();
  initDepthPassPipelineLayout();
  initTimer();
  initEnvMapRenderPassDataBuffer();

  waitDeviceIdle();

  IsInitialized = true;

  _vkCmdSetVertexInput = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(
      vkGetDeviceProcAddr(_vkDevice, "vkCmdSetVertexInputEXT"));
}

void VulkanRHI::initResources(rhi::InitResourcesRHI const& initResources) {
  assert(IsInitialized);

  std::vector<rhi::ResourceTransferRHI> transfers;

  _depthPassVertexShaderId = initShaderResource().id;
  transfers.emplace_back(uploadShader(_depthPassVertexShaderId,
                                      initResources.shadowPassVertexShader));

  _depthPassFragmentShaderId = initShaderResource().id;
  transfers.emplace_back(uploadShader(_depthPassFragmentShaderId,
                                      initResources.shadowPassFragmentShader));

  _ssaoVertexShaderId = initShaderResource().id;
  transfers.emplace_back(
      uploadShader(_ssaoVertexShaderId, initResources.ssaoVertexShader));

  _ssaoFragmentShaderId = initShaderResource().id;
  transfers.emplace_back(
      uploadShader(_ssaoFragmentShaderId, initResources.ssaoFragmentShader));

  _postProcessingVertexShaderId = initShaderResource().id;
  transfers.emplace_back(uploadShader(
      _postProcessingVertexShaderId, initResources.postProcessingVertexShader));

  _postProcessingFragmentShaderId = initShaderResource().id;
  transfers.emplace_back(
      uploadShader(_postProcessingFragmentShaderId,
                   initResources.postProcessingFragmentShader));

  _deletionQueue.pushFunction([this]() {
    releaseShader(_depthPassVertexShaderId);
    releaseShader(_depthPassFragmentShaderId);
    releaseShader(_ssaoVertexShaderId);
    releaseShader(_ssaoFragmentShaderId);
    releaseShader(_postProcessingVertexShaderId);
    releaseShader(_postProcessingFragmentShaderId);
  });

  for (rhi::ResourceTransferRHI& t : transfers) {
    assert(t.transferStarted());
    t.waitCompleted();
  }

  initDepthPrepassPipeline();
  initShadowPassPipeline();
  initSsaoPipeline();
  initSsaoPostProcessingPipeline();
}

void VulkanRHI::initVulkan(rhi::ISurfaceProviderRHI const& surfaceProvider) {
  vkb::InstanceBuilder builder;

#ifdef USE_VULKAN_VALIDATION_LAYERS
  constexpr bool enable_validation_layers = true;
#else
  constexpr bool enable_validation_layers = false;
#endif

  auto builderReturn = builder.set_app_name("Obsidian Engine")
                           .request_validation_layers(enable_validation_layers)
                           .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                           .require_api_version(1, 2, 0)
                           .use_default_debug_messenger()
                           .build();

  vkb::Instance vkbInstance = builderReturn.value();

  _vkInstance = vkbInstance.instance;
  _vkDebugMessenger = vkbInstance.debug_messenger;

  initDebugUtils(_vkInstance);

  surfaceProvider.provideSurface(*this);

  VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures = {};
  vkPhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

  vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice =
      vkbSelector.set_minimum_version(1, 2)
          .set_surface(_vkSurface)
          .add_required_extension(
              VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME)
          .add_required_extension(
              VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)
          .set_required_features(vkPhysicalDeviceFeatures)
          .select()
          .value();

  _maxSamplerAnisotropy =
      vkbPhysicalDevice.properties.limits.maxSamplerAnisotropy;
  vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
  VkPhysicalDeviceVulkan12Features vkDeviceFeatures = {};
  vkDeviceFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vkDeviceFeatures.pNext = nullptr;
  vkDeviceFeatures.descriptorBindingPartiallyBound = VK_TRUE;

  vkbDeviceBuilder.add_pNext(&vkDeviceFeatures);

  VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT
      vkVertexInputDynamicStateFeatures = {};
  vkVertexInputDynamicStateFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
  vkVertexInputDynamicStateFeatures.vertexInputDynamicState = true;

  vkbDeviceBuilder.add_pNext(&vkVertexInputDynamicStateFeatures);

  vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

  _vkDevice = vkbDevice.device;
  _vkPhysicalDevice = vkbPhysicalDevice.physical_device;
  _vkPhysicalDeviceProperties = vkbPhysicalDevice.properties;

  _graphicsQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
  _gpuQueues[_graphicsQueueFamilyIndex] =
      vkbDevice.get_queue(vkb::QueueType::graphics).value();
  // init mutex
  _gpuQueueMutexes[_graphicsQueueFamilyIndex];

  _transferQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
  _gpuQueues[_transferQueueFamilyIndex] =
      vkbDevice.get_queue(vkb::QueueType::transfer).value();
  // init mutex
  _gpuQueueMutexes[_transferQueueFamilyIndex];

  _queueFamilyIndices.emplace(_graphicsQueueFamilyIndex);
  _queueFamilyIndices.emplace(_transferQueueFamilyIndex);

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _vkPhysicalDevice;
  allocatorInfo.device = _vkDevice;
  allocatorInfo.instance = _vkInstance;
  VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_vmaAllocator));

  _deletionQueue.pushFunction([this] { vmaDestroyAllocator(_vmaAllocator); });
}

void VulkanRHI::initSwapchain(rhi::WindowExtentRHI const& extent) {
  vkb::SwapchainBuilder swapchainBuilder{_vkPhysicalDevice, _vkDevice,
                                         _vkSurface};
  swapchainBuilder.use_default_format_selection()
      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
      .set_desired_extent(extent.width, extent.height);

  swapchainBuilder.set_old_swapchain(_vkbSwapchain.swapchain);

  vkb::Swapchain oldSwapchain = _vkbSwapchain;

  _vkbSwapchain = swapchainBuilder.build().value();

  _swapchainImages = _vkbSwapchain.get_images().value();
  _swapchainImageViews = _vkbSwapchain.get_image_views().value();
  _swapchainDeletionQueue.pushFunction(
      [this]() { _vkbSwapchain.destroy_image_views(_swapchainImageViews); });

  vkb::destroy_swapchain(oldSwapchain);
}

void VulkanRHI::initCommands() {
  VkCommandPoolCreateInfo vkCommandPoolCreateInfo =
      vkinit::commandPoolCreateInfo(
          _graphicsQueueFamilyIndex,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < _frameDataArray.size(); ++i) {
    FrameData& frameData = _frameDataArray[i];
    VK_CHECK(vkCreateCommandPool(_vkDevice, &vkCommandPoolCreateInfo, nullptr,
                                 &frameData.vkCommandPool));

    _deletionQueue.pushFunction([this, frameData]() {
      vkDestroyCommandPool(_vkDevice, frameData.vkCommandPool, nullptr);
    });

    constexpr std::uint32_t commandPoolCount = 1;
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo =
        vkinit::commandBufferAllocateInfo(frameData.vkCommandPool,
                                          commandPoolCount,
                                          VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VK_CHECK(vkAllocateCommandBuffers(_vkDevice, &vkCommandBufferAllocateInfo,
                                      &frameData.vkCommandBuffer));
  }
}

void VulkanRHI::initMainRenderPasses() {
  RenderPassBuilder::begin(_vkDevice)
      .setColorAttachment(_vkbSwapchain.image_format,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
      .setDepthAttachment(_depthFormat, false)
      .setColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .setDepthSubpassReference(0,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .build(_mainRenderPassReuseDepth)

      .setColorAttachment(_envMapFormat,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      .setDepthAttachment(_depthFormat, true)
      .setDepthSubpassReference(
          0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .build(_envMapRenderPass);

  setDbgResourceName(
      _vkDevice, (std::uint64_t)_mainRenderPassReuseDepth.vkRenderPass,
      VK_OBJECT_TYPE_RENDER_PASS, "Main render pass (reuse depth)");
  setDbgResourceName(_vkDevice, (std::uint64_t)_envMapRenderPass.vkRenderPass,
                     VK_OBJECT_TYPE_RENDER_PASS, "Env map render pass");

  _swapchainDeletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _mainRenderPassReuseDepth.vkRenderPass,
                        nullptr);
    vkDestroyRenderPass(_vkDevice, _envMapRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initDepthRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .setDepthAttachment(_depthFormat)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .setDepthSubpassReference(
          0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .build(_depthRenderPass);

  setDbgResourceName(_vkDevice, (std::uint64_t)_depthRenderPass.vkRenderPass,
                     VK_OBJECT_TYPE_RENDER_PASS, "Depth render pass");

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _depthRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initSsaoRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .setColorAttachment(_ssaoFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .setDepthAttachment(_depthFormat, true)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .setColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .setDepthSubpassReference(
          0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .build(_ssaoRenderPass);

  setDbgResourceName(_vkDevice, (std::uint64_t)_ssaoRenderPass.vkRenderPass,
                     VK_OBJECT_TYPE_RENDER_PASS, "Ssao render pass");

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _ssaoRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initPostProcessingRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .setColorAttachment(_ssaoFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .setColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .build(_postProcessingRenderPass);

  setDbgResourceName(_vkDevice,
                     (std::uint64_t)_postProcessingRenderPass.vkRenderPass,
                     VK_OBJECT_TYPE_RENDER_PASS, "Post processing render pass");

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _postProcessingRenderPass.vkRenderPass,
                        nullptr);
  });
}

void VulkanRHI::initSwapchainFramebuffers() {
  _vkSwapchainFramebuffers.resize(_vkbSwapchain.image_count);

  for (int i = 0; i < _vkbSwapchain.image_count; ++i) {
    for (int j = 0; j < frameOverlap; ++j) {
      _vkSwapchainFramebuffers[i][j] =
          _mainRenderPassReuseDepth.generateFramebuffer(
              _vmaAllocator, _vkbSwapchain.extent, {}, _swapchainImageViews[i],
              _frameDataArray[j]
                  .vkDepthPrepassFramebuffer.depthBufferImageView);

      nameFramebufferResources(_vkDevice, _vkSwapchainFramebuffers[i][j],
                               "Post processing");

      _swapchainDeletionQueue.pushFunction(
          [this, &framebuffer = _vkSwapchainFramebuffers[i][j]]() {
            vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
          });
    }
  }
}

void VulkanRHI::initDepthPrepassFramebuffers() {
  for (FrameData& frameData : _frameDataArray) {
    frameData.vkDepthPrepassFramebuffer = _depthRenderPass.generateFramebuffer(
        _vmaAllocator, _vkbSwapchain.extent,
        {.depthImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

    nameFramebufferResources(_vkDevice, frameData.vkDepthPrepassFramebuffer,
                             "Depth prepass");

    _swapchainDeletionQueue.pushFunction(
        [this, &framebuffer = frameData.vkDepthPrepassFramebuffer]() {
          vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
          vkDestroyImageView(_vkDevice, framebuffer.depthBufferImageView,
                             nullptr);
          vmaDestroyImage(_vmaAllocator, framebuffer.depthBufferImage.vkImage,
                          framebuffer.depthBufferImage.allocation);
        });

    VkImageCreateInfo depthPrepassResultImgCreateInfo = vkinit::imageCreateInfo(
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        {_vkbSwapchain.extent.width, _vkbSwapchain.extent.height, 1},
        _depthFormat);
    depthPrepassResultImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthPrepassResultAllocationCreateInfo = {};
    depthPrepassResultAllocationCreateInfo.usage =
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(
        _vmaAllocator, &depthPrepassResultImgCreateInfo,
        &depthPrepassResultAllocationCreateInfo,
        &frameData.depthPassResultShaderReadImage.vkImage,
        &frameData.depthPassResultShaderReadImage.allocation, nullptr));

    VkImageViewCreateInfo const depthPrepassResultImgViewCreateInfo =
        vkinit::imageViewCreateInfo(
            frameData.depthPassResultShaderReadImage.vkImage, _depthFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(
        _vkDevice, &depthPrepassResultImgViewCreateInfo, nullptr,
        &frameData.vkDepthPassResultShaderReadImageView));

    _swapchainDeletionQueue.pushFunction([this, &frameData]() {
      vkDestroyImageView(
          _vkDevice, frameData.vkDepthPassResultShaderReadImageView, nullptr);
      vmaDestroyImage(_vmaAllocator,
                      frameData.depthPassResultShaderReadImage.vkImage,
                      frameData.depthPassResultShaderReadImage.allocation);
    });
  }
}

void VulkanRHI::initShadowPassFramebuffers() {
  for (std::size_t i = 0; i < _frameDataArray.size(); ++i) {
    for (std::size_t j = 0; j < rhi::maxLightsPerDrawPass; ++j) {
      AllocatedImage const& imageShadowPassAttachment =
          _frameDataArray[i].shadowFrameBuffers[j].depthBufferImage;

      VkExtent2D const extent = {shadowPassAttachmentWidth,
                                 shadowPassAttachmentHeight};

      _frameDataArray[i].shadowFrameBuffers[j] =
          _depthRenderPass.generateFramebuffer(
              _vmaAllocator, extent,
              {.depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT});

      nameFramebufferResources(
          _vkDevice, _frameDataArray[i].shadowFrameBuffers[j], "Shadow pass");

      _deletionQueue.pushFunction(
          [this, &framebuffer = _frameDataArray[i].shadowFrameBuffers[j]]() {
            vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
            vkDestroyImageView(_vkDevice, framebuffer.depthBufferImageView,
                               nullptr);
            vmaDestroyImage(_vmaAllocator, framebuffer.depthBufferImage.vkImage,
                            framebuffer.depthBufferImage.allocation);
          });
    }
  }
}

void VulkanRHI::initGlobalSettingsBuffer() {
  _globalSettingsBuffer = createBuffer(
      getPaddedBufferSize(sizeof(GPUGlobalSettings)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  updateGlobalSettingsBuffer(true);

  setDbgResourceName(_vkDevice, (std::uint64_t)_globalSettingsBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Global settings buffer");

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _globalSettingsBuffer.buffer,
                     _globalSettingsBuffer.allocation);
  });
}

void VulkanRHI::initSsaoFramebuffers() {
  VkExtent2D const ssaoExtent = getSsaoExtent();

  for (FrameData& frameData : _frameDataArray) {
    frameData.vkSsaoFramebuffer = _ssaoRenderPass.generateFramebuffer(
        _vmaAllocator, ssaoExtent,
        {.colorImageUsage =
             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
         .depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

    nameFramebufferResources(_vkDevice, frameData.vkSsaoFramebuffer, "Ssao");

    _swapchainDeletionQueue.pushFunction([this,
                                          &framebuffer =
                                              frameData.vkSsaoFramebuffer]() {
      vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
      vkDestroyImageView(_vkDevice, framebuffer.colorBufferImageView, nullptr);
      vmaDestroyImage(_vmaAllocator, framebuffer.colorBufferImage.vkImage,
                      framebuffer.colorBufferImage.allocation);

      vkDestroyImageView(_vkDevice, framebuffer.depthBufferImageView, nullptr);
      vmaDestroyImage(_vmaAllocator, framebuffer.depthBufferImage.vkImage,
                      framebuffer.depthBufferImage.allocation);
    });
  }
}

void VulkanRHI::initSsaoPostProcessingFramebuffers() {
  for (FrameData& frameData : _frameDataArray) {
    frameData.vkSsaoPostProcessingFramebuffer =
        _postProcessingRenderPass.generateFramebuffer(
            _vmaAllocator, getSsaoExtent(),
            {.colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT});

    nameFramebufferResources(_vkDevice, frameData.vkSsaoFramebuffer,
                             "Post processing");

    _swapchainDeletionQueue.pushFunction(
        [this, &frameBuffer = frameData.vkSsaoPostProcessingFramebuffer]() {
          vkDestroyFramebuffer(_vkDevice, frameBuffer.vkFramebuffer, nullptr);
          vkDestroyImageView(_vkDevice, frameBuffer.colorBufferImageView,
                             nullptr);
          vmaDestroyImage(_vmaAllocator, frameBuffer.colorBufferImage.vkImage,
                          frameBuffer.colorBufferImage.allocation);
        });
  }
}

void VulkanRHI::initSyncStructures() {
  VkFenceCreateInfo vkFenceCreateInfo =
      vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

  for (int i = 0; i < _frameDataArray.size(); ++i) {
    FrameData& frameData = _frameDataArray[i];

    VK_CHECK(vkCreateFence(_vkDevice, &vkFenceCreateInfo, nullptr,
                           &frameData.vkRenderFence));

    _deletionQueue.pushFunction([this, frameData]() {
      vkDestroyFence(_vkDevice, frameData.vkRenderFence, nullptr);
    });

    VkSemaphoreCreateInfo vkSemaphoreCreateInfo =
        vkinit::semaphoreCreateInfo(0);

    VK_CHECK(vkCreateSemaphore(_vkDevice, &vkSemaphoreCreateInfo, nullptr,
                               &frameData.vkRenderSemaphore));
    VK_CHECK(vkCreateSemaphore(_vkDevice, &vkSemaphoreCreateInfo, nullptr,
                               &frameData.vkPresentSemaphore));

    _deletionQueue.pushFunction([this, frameData]() {
      vkDestroySemaphore(_vkDevice, frameData.vkRenderSemaphore, nullptr);
      vkDestroySemaphore(_vkDevice, frameData.vkPresentSemaphore, nullptr);
    });
  }
}

void VulkanRHI::initMainPipelineAndLayouts() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);
  pipelineBuilder._vkColorBlendAttachmentState =
      vkinit::colorBlendAttachmentState();
  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  VkPipelineLayoutCreateInfo meshPipelineLayoutInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> const meshDescriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkMainRenderPassDescriptorSetLayout,
      _vkUnlitTexturedMaterialDescriptorSetLayout, _objectDescriptorSetLayout};

  meshPipelineLayoutInfo.setLayoutCount = meshDescriptorSetLayouts.size();
  meshPipelineLayoutInfo.pSetLayouts = meshDescriptorSetLayouts.data();

  VkPushConstantRange vkPushConstantRange;
  vkPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  vkPushConstantRange.offset = 0;
  vkPushConstantRange.size = sizeof(MeshPushConstants);

  meshPipelineLayoutInfo.pushConstantRangeCount = 1;
  meshPipelineLayoutInfo.pPushConstantRanges = &vkPushConstantRange;

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &meshPipelineLayoutInfo, nullptr,
                                  &_vkMeshPipelineLayout));

  pipelineBuilder._vkPipelineLayout = _vkMeshPipelineLayout;

  _pipelineBuilders[core::MaterialType::unlit] = pipelineBuilder;

  pipelineBuilder._vkShaderStageCreateInfos.clear();

  _deletionQueue.pushFunction([this] {
    vkDestroyPipelineLayout(_vkDevice, _vkMeshPipelineLayout, nullptr);
  });

  // Lit mesh pipeline
  VkPipelineLayoutCreateInfo litMeshPipelineLayoutCreateInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> vkLitMeshPipelineLayouts = {
      _vkGlobalDescriptorSetLayout,
      _vkMainRenderPassDescriptorSetLayout,
      _vkLitTexturedMaterialDescriptorSetLayout,
      _objectDescriptorSetLayout,
  };

  litMeshPipelineLayoutCreateInfo.pSetLayouts = vkLitMeshPipelineLayouts.data();
  litMeshPipelineLayoutCreateInfo.setLayoutCount =
      vkLitMeshPipelineLayouts.size();
  litMeshPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  litMeshPipelineLayoutCreateInfo.pPushConstantRanges = &vkPushConstantRange;
  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &litMeshPipelineLayoutCreateInfo,
                                  nullptr, &_vkLitMeshPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkLitMeshPipelineLayout, nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkLitMeshPipelineLayout;

  _pipelineBuilders[core::MaterialType::lit] = pipelineBuilder;

  // pbr mesh pipeline

  VkPipelineLayoutCreateInfo pbrMeshPipelineLayoutCreateInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> vkPbrPipelineLayouts = {
      _vkGlobalDescriptorSetLayout,
      _vkMainRenderPassDescriptorSetLayout,
      _vkPbrMaterialDescriptorSetLayout,
      _objectDescriptorSetLayout,
  };

  pbrMeshPipelineLayoutCreateInfo.pSetLayouts = vkPbrPipelineLayouts.data();
  pbrMeshPipelineLayoutCreateInfo.setLayoutCount = vkPbrPipelineLayouts.size();
  pbrMeshPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pbrMeshPipelineLayoutCreateInfo.pPushConstantRanges = &vkPushConstantRange;
  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &pbrMeshPipelineLayoutCreateInfo,
                                  nullptr, &_vkPbrMeshPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkPbrMeshPipelineLayout, nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkPbrMeshPipelineLayout;

  _pipelineBuilders[core::MaterialType::pbr] = pipelineBuilder;

  pipelineBuilder._vkShaderStageCreateInfos.clear();
}

void VulkanRHI::initDepthPassPipelineLayout() {
  VkPipelineLayoutCreateInfo vkDepthPipelineLayoutCreateInfo = {};
  vkDepthPipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  vkDepthPipelineLayoutCreateInfo.pNext = nullptr;

  std::array<VkDescriptorSetLayout, 4> depthDescriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkDepthPassDescriptorSetLayout,
      _vkEmptyDescriptorSetLayout, _vkEmptyDescriptorSetLayout};

  vkDepthPipelineLayoutCreateInfo.setLayoutCount =
      depthDescriptorSetLayouts.size();
  vkDepthPipelineLayoutCreateInfo.pSetLayouts =
      depthDescriptorSetLayouts.data();

  VkPushConstantRange vkPushConstantRange;
  vkPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  vkPushConstantRange.offset = 0;

  constexpr std::size_t pushConstantSize = sizeof(MeshPushConstants);
  static_assert(pushConstantSize <= 128 &&
                "Push constants should not exceed size of 128 bytes to ensure "
                "portability");
  vkPushConstantRange.size = pushConstantSize;

  vkDepthPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  vkDepthPipelineLayoutCreateInfo.pPushConstantRanges = &vkPushConstantRange;

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &vkDepthPipelineLayoutCreateInfo,
                                  nullptr, &_vkDepthPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkDepthPipelineLayout, nullptr);
  });
}

void VulkanRHI::initShadowPassPipeline() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, true);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);

  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

  pipelineBuilder._vkViewport.x = 0.f;
  pipelineBuilder._vkViewport.y = 0.f;
  pipelineBuilder._vkViewport.width = shadowPassAttachmentWidth;
  pipelineBuilder._vkViewport.height = shadowPassAttachmentHeight;
  pipelineBuilder._vkViewport.minDepth = 0.0f;
  pipelineBuilder._vkViewport.maxDepth = 1.0f;

  pipelineBuilder._vkScissor.offset = {0, 0};
  pipelineBuilder._vkScissor.extent = {shadowPassAttachmentWidth,
                                       shadowPassAttachmentHeight};

  VkShaderModule const vertexShaderModule =
      _shaderModules[_depthPassVertexShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            vertexShaderModule));

  VkShaderModule const fragmentShaderModule =
      _shaderModules[_depthPassFragmentShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            fragmentShaderModule));

  pipelineBuilder._vkPipelineLayout = _vkDepthPipelineLayout;

  _vkShadowPassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _depthRenderPass.vkRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipeline(_vkDevice, _vkShadowPassPipeline, nullptr);
  });
}

void VulkanRHI::initDepthPrepassPipeline() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, true);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);

  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

  VkShaderModule const vertexShaderModule =
      _shaderModules[_depthPassVertexShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            vertexShaderModule));

  VkShaderModule const fragmentShaderModule =
      _shaderModules[_depthPassFragmentShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            fragmentShaderModule));

  pipelineBuilder._vkPipelineLayout = _vkDepthPipelineLayout;

  pipelineBuilder._vkColorBlendAttachmentState.blendEnable = VK_FALSE;

  _vkDepthPrepassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _depthRenderPass.vkRenderPass);

  _deletionQueue.pushFunction([this] {
    vkDestroyPipeline(_vkDevice, _vkDepthPrepassPipeline, nullptr);
  });
}

void VulkanRHI::initSsaoPipeline() {
  PipelineBuilder pipelineBuilder = {};

  pipelineBuilder._vkShaderStageCreateInfos.reserve(2);

  VkShaderModule const ssaoVertexShaderModule =
      _shaderModules[_ssaoVertexShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            ssaoVertexShaderModule));

  VkShaderModule const ssaoFragmentShaderModule =
      _shaderModules[_ssaoFragmentShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            ssaoFragmentShaderModule));
  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, true);
  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);
  pipelineBuilder._vkColorBlendAttachmentState.blendEnable = false;
  pipelineBuilder._vkColorBlendAttachmentState.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT;
  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.pNext = nullptr;

  std::array<VkDescriptorSetLayout, 4> ssaoDescriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkSsaoDescriptorSetLayout,
      _vkEmptyDescriptorSetLayout, _vkEmptyDescriptorSetLayout};

  pipelineLayoutCreateInfo.setLayoutCount = ssaoDescriptorSetLayouts.size();
  pipelineLayoutCreateInfo.pSetLayouts = ssaoDescriptorSetLayouts.data();

  VkPushConstantRange pushConstantRange;
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRange.offset = 0;

  constexpr std::size_t pushConstantSize = sizeof(GPUObjectData);
  static_assert(pushConstantSize <= 128 &&
                "Push constants should not exceed size of 128 bytes to ensure "
                "portability");
  pushConstantRange.size = pushConstantSize;

  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &pipelineLayoutCreateInfo, nullptr,
                                  &_vkSsaoPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkSsaoPipelineLayout, nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkSsaoPipelineLayout;

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

  _vkSsaoPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _ssaoRenderPass.vkRenderPass);

  _deletionQueue.pushFunction(
      [this]() { vkDestroyPipeline(_vkDevice, _vkSsaoPipeline, nullptr); });
}

void VulkanRHI::initSsaoPostProcessingPipeline() {
  PipelineBuilder pipelineBuilder = {};

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_VERTEX_BIT,
          _shaderModules[_postProcessingVertexShaderId].vkShaderModule));
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_FRAGMENT_BIT,
          _shaderModules[_postProcessingFragmentShaderId].vkShaderModule));

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(false, false);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);

  pipelineBuilder._vkColorBlendAttachmentState = {};
  pipelineBuilder._vkColorBlendAttachmentState.blendEnable = VK_FALSE;
  pipelineBuilder._vkColorBlendAttachmentState.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT;

  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  VkPipelineLayoutCreateInfo layoutCreateInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> descriptorSetLayouts = {
      _vkEmptyDescriptorSetLayout, _vkSsaoPostProcessingDescriptorSetLayout,
      _vkEmptyDescriptorSetLayout, _vkEmptyDescriptorSetLayout};
  layoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
  layoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

  VkPushConstantRange pushConstantRange;
  pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;

  constexpr std::size_t pushConstantSize = sizeof(PostProcessingPushConstants);
  static_assert(pushConstantSize <= 128 &&
                "Push constants should not exceed size of 128 bytes to ensure "
                "portability");
  pushConstantRange.size = pushConstantSize;

  layoutCreateInfo.pushConstantRangeCount = 1;
  layoutCreateInfo.pPushConstantRanges = &pushConstantRange;

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &layoutCreateInfo, nullptr,
                                  &_vkSsaoPostProcessingPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkSsaoPostProcessingPipelineLayout,
                            nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkSsaoPostProcessingPipelineLayout;

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

  _vkSsaoPostProcessingPipeline = pipelineBuilder.buildPipeline(
      _vkDevice, _postProcessingRenderPass.vkRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipeline(_vkDevice, _vkSsaoPostProcessingPipeline, nullptr);
  });
}

void VulkanRHI::initDescriptorBuilder() {
  _descriptorLayoutCache.init(_vkDevice);
  _descriptorAllocator.init(_vkDevice);
  _swapchainBoundDescriptorAllocator.init(_vkDevice);

  _deletionQueue.pushFunction([this]() {
    _descriptorAllocator.cleanup();
    _descriptorLayoutCache.cleanup();
    _swapchainBoundDescriptorAllocator.cleanup();
  });
}

void VulkanRHI::initDefaultSamplers() {
  VkSamplerCreateInfo diffuseSamplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, _maxSamplerAnisotropy);

  VK_CHECK(vkCreateSampler(_vkDevice, &diffuseSamplerCreateInfo, nullptr,
                           &_vkLinearRepeatSampler));

  _deletionQueue.pushFunction([this]() {
    vkDestroySampler(_vkDevice, _vkLinearRepeatSampler, nullptr);
  });
}

void VulkanRHI::initMainRenderPassDataBuffer() {
  _mainRenderPassDataBuffer = createBuffer(
      getPaddedBufferSize(sizeof(GpuRenderPassData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
  setDbgResourceName(_vkDevice, (std::uint64_t)_mainRenderPassDataBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Main render pass data");

  GpuRenderPassData data{VK_TRUE};

  uploadBufferData(0, data, _mainRenderPassDataBuffer, VK_QUEUE_FAMILY_IGNORED);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _mainRenderPassDataBuffer.buffer,
                     _mainRenderPassDataBuffer.allocation);
  });
}

void VulkanRHI::initLightDataBuffer() {
  _lightDataBuffer = createBuffer(
      frameOverlap * getPaddedBufferSize(sizeof(GPULightData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_lightDataBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Light data buffer");

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _lightDataBuffer.buffer,
                     _lightDataBuffer.allocation);
  });
}

void VulkanRHI::initDepthSampler() {
  VkSamplerCreateInfo vkDepthSamplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
  vkDepthSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  VK_CHECK(vkCreateSampler(_vkDevice, &vkDepthSamplerCreateInfo, nullptr,
                           &_vkDepthSampler));

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _vkDepthSampler, nullptr); });
}

void VulkanRHI::initDescriptors() {
  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .build(_emptyDescriptorSet, _vkEmptyDescriptorSetLayout);

  std::size_t const paddedSceneDataSize =
      getPaddedBufferSize(sizeof(GPUSceneData));
  std::size_t const sceneDataBufferSize = frameOverlap * paddedSceneDataSize;
  _sceneDataBuffer = createBuffer(sceneDataBufferSize,
                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_AUTO, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_sceneDataBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Scene data");

  _swapchainDeletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _sceneDataBuffer.buffer,
                     _sceneDataBuffer.allocation);
  });

  _cameraBuffer = createBuffer(
      frameOverlap * getPaddedBufferSize(sizeof(GPUCameraData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_cameraBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Camera data");

  _swapchainDeletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _cameraBuffer.buffer,
                     _cameraBuffer.allocation);
    _cameraBuffer = {};
  });

  VkDescriptorBufferInfo sceneDescriptorBufferInfo;
  sceneDescriptorBufferInfo.buffer = _sceneDataBuffer.buffer;
  sceneDescriptorBufferInfo.offset = 0;
  sceneDescriptorBufferInfo.range = sizeof(GPUSceneData);

  VkDescriptorBufferInfo envMapDataDescriptorBufferInfo;
  envMapDataDescriptorBufferInfo.buffer = _envMapDataBuffer.buffer;
  envMapDataDescriptorBufferInfo.offset = 0;
  envMapDataDescriptorBufferInfo.range =
      sizeof(GpuEnvironmentMapDataCollection);

  VkDescriptorBufferInfo globalSettingsDescriptorBufferInfo;
  globalSettingsDescriptorBufferInfo.buffer = _globalSettingsBuffer.buffer;
  globalSettingsDescriptorBufferInfo.offset = 0;
  globalSettingsDescriptorBufferInfo.range = sizeof(GPUGlobalSettings);

  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, sceneDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_FRAGMENT_BIT)
      .bindBuffer(1, envMapDataDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                  VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true)
      .declareUnusedImage(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          VK_SHADER_STAGE_FRAGMENT_BIT, nullptr,
                          maxEnvironmentMaps)
      .bindBuffer(3, globalSettingsDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                  VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(_vkGlobalDescriptorSet, _vkGlobalDescriptorSetLayout);

  for (int i = 0; i < frameOverlap; ++i) {
    FrameData& frameData = _frameDataArray[i];

    std::vector<VkDescriptorImageInfo> vkShadowMapDescriptorImageInfos{
        rhi::maxLightsPerDrawPass};

    for (std::size_t j = 0; j < vkShadowMapDescriptorImageInfos.size(); ++j) {
      vkShadowMapDescriptorImageInfos[j].imageLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      vkShadowMapDescriptorImageInfos[j].imageView =
          frameData.shadowFrameBuffers[j].depthBufferImageView;
      vkShadowMapDescriptorImageInfos[j].sampler = _vkDepthSampler;
    }

    VkDescriptorBufferInfo renderPassDataDescriptorBufferInfo = {};
    renderPassDataDescriptorBufferInfo.buffer =
        _mainRenderPassDataBuffer.buffer;
    renderPassDataDescriptorBufferInfo.offset = 0;
    renderPassDataDescriptorBufferInfo.range = sizeof(GpuRenderPassData);

    VkDescriptorBufferInfo cameraDescriptorBufferInfo = {};
    cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
    cameraDescriptorBufferInfo.offset = 0;
    cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo vkLightDataBufferInfo = {};
    vkLightDataBufferInfo.buffer = _lightDataBuffer.buffer;
    vkLightDataBufferInfo.offset = 0;
    vkLightDataBufferInfo.range = sizeof(GPULightData);

    VkDescriptorImageInfo vkSsaoImageInfo = {};
    vkSsaoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkSsaoImageInfo.sampler = _vkLinearRepeatSampler;
    vkSsaoImageInfo.imageView =
        frameData.vkSsaoPostProcessingFramebuffer.colorBufferImageView;

    DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                             _descriptorLayoutCache)
        .bindBuffer(0, renderPassDataDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(1, cameraDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImages(2, vkShadowMapDescriptorImageInfos,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(3, vkLightDataBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImage(4, vkSsaoImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, true)
        .build(frameData.vkMainRenderPassDescriptorSet,
               _vkMainRenderPassDescriptorSetLayout);
  }

  {
    std::array<VkDescriptorSetLayoutBinding, 4> litMaterialBinding = {};
    litMaterialBinding[0].binding = 0;
    litMaterialBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    litMaterialBinding[0].descriptorCount = 1;
    litMaterialBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    litMaterialBinding[1].binding = 1;
    litMaterialBinding[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    litMaterialBinding[1].descriptorCount = 1;
    litMaterialBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    litMaterialBinding[2].binding = 2;
    litMaterialBinding[2].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    litMaterialBinding[2].descriptorCount = 1;
    litMaterialBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    litMaterialBinding[3].binding = 10;
    litMaterialBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    litMaterialBinding[3].descriptorCount = 1;
    litMaterialBinding[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorBindingFlags, 4> bindingFlags = {
        0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pNext = nullptr;
    bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo litTexMatDescriptorSetLayoutCreateInfo = {};
    litTexMatDescriptorSetLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    litTexMatDescriptorSetLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;

    litTexMatDescriptorSetLayoutCreateInfo.bindingCount =
        litMaterialBinding.size();
    litTexMatDescriptorSetLayoutCreateInfo.pBindings =
        litMaterialBinding.data();
    _vkLitTexturedMaterialDescriptorSetLayout =
        _descriptorLayoutCache.getLayout(
            litTexMatDescriptorSetLayoutCreateInfo);
  }

  {
    std::array<VkDescriptorSetLayoutBinding, 3> unlitMaterialBinding = {};

    unlitMaterialBinding[0].binding = 0;
    unlitMaterialBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    unlitMaterialBinding[0].descriptorCount = 1;
    unlitMaterialBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    unlitMaterialBinding[1].binding = 1;
    unlitMaterialBinding[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    unlitMaterialBinding[1].descriptorCount = 1;
    unlitMaterialBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    unlitMaterialBinding[2].binding = 10;
    unlitMaterialBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    unlitMaterialBinding[2].descriptorCount = 1;
    unlitMaterialBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorBindingFlags, 3> bindingFlags = {
        0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pNext = nullptr;
    bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo unlitTexMatDescriptorSetLayoutCreateInfo =
        {};
    unlitTexMatDescriptorSetLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    unlitTexMatDescriptorSetLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;
    unlitTexMatDescriptorSetLayoutCreateInfo.bindingCount =
        unlitMaterialBinding.size();
    unlitTexMatDescriptorSetLayoutCreateInfo.pBindings =
        unlitMaterialBinding.data();
    _vkUnlitTexturedMaterialDescriptorSetLayout =
        _descriptorLayoutCache.getLayout(
            unlitTexMatDescriptorSetLayoutCreateInfo);
  }

  {
    std::array<VkDescriptorSetLayoutBinding, 6> pbrMaterialBinding = {};

    pbrMaterialBinding[0].binding = 0;
    pbrMaterialBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pbrMaterialBinding[0].descriptorCount = 1;
    pbrMaterialBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    pbrMaterialBinding[1].binding = 1;
    pbrMaterialBinding[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pbrMaterialBinding[1].descriptorCount = 1;
    pbrMaterialBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    pbrMaterialBinding[2].binding = 2;
    pbrMaterialBinding[2].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pbrMaterialBinding[2].descriptorCount = 1;
    pbrMaterialBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    pbrMaterialBinding[3].binding = 3;
    pbrMaterialBinding[3].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pbrMaterialBinding[3].descriptorCount = 1;
    pbrMaterialBinding[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    pbrMaterialBinding[4].binding = 4;
    pbrMaterialBinding[4].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pbrMaterialBinding[4].descriptorCount = 1;
    pbrMaterialBinding[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    pbrMaterialBinding[5].binding = 10;
    pbrMaterialBinding[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pbrMaterialBinding[5].descriptorCount = 1;
    pbrMaterialBinding[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorBindingFlags, pbrMaterialBinding.size()>
        bindingFlags = {0,
                        0,
                        0,
                        0,
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pNext = nullptr;
    bindingFlagsCreateInfo.bindingCount = bindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo pbrDescriptorSetLayoutCreaInfo = {};
    pbrDescriptorSetLayoutCreaInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    pbrDescriptorSetLayoutCreaInfo.pNext = &bindingFlagsCreateInfo;
    pbrDescriptorSetLayoutCreaInfo.bindingCount = pbrMaterialBinding.size();
    pbrDescriptorSetLayoutCreaInfo.pBindings = pbrMaterialBinding.data();

    _vkPbrMaterialDescriptorSetLayout =
        _descriptorLayoutCache.getLayout(pbrDescriptorSetLayoutCreaInfo);
  }

  {
    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .getLayout(_objectDescriptorSetLayout);
  }
}

void VulkanRHI::initDepthPrepassDescriptors() {
  VkDescriptorBufferInfo cameraBufferInfo;
  cameraBufferInfo.buffer = _cameraBuffer.buffer;
  cameraBufferInfo.offset = 0;
  cameraBufferInfo.range = getPaddedBufferSize(sizeof(GPUCameraData));

  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(1, cameraBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_depthPrepassDescriptorSet, _vkDepthPassDescriptorSetLayout);
}

void VulkanRHI::initShadowPassDescriptors() {
  _shadowPassCameraBuffer = createBuffer(
      frameOverlap * rhi::maxLightsPerDrawPass *
          getPaddedBufferSize(sizeof(GPUCameraData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_shadowPassCameraBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Shadow pass camera data");

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _shadowPassCameraBuffer.buffer,
                     _shadowPassCameraBuffer.allocation);
  });

  VkDescriptorBufferInfo vkCameraDatabufferInfo = {};
  vkCameraDatabufferInfo.buffer = _shadowPassCameraBuffer.buffer;
  vkCameraDatabufferInfo.offset = 0;
  vkCameraDatabufferInfo.range = sizeof(GPUCameraData);
  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(1, vkCameraDatabufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_vkShadowPassDescriptorSet);
}

void VulkanRHI::initSsaoDescriptors() {
  for (FrameData& frameData : _frameDataArray) {
    VkDescriptorBufferInfo cameraDescriptorBufferInfo = {};
    cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
    cameraDescriptorBufferInfo.offset = 0;
    cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo samplesDescriptorBufferInfo = {};
    samplesDescriptorBufferInfo.buffer = _ssaoSamplesBuffer.buffer;
    samplesDescriptorBufferInfo.offset = 0;
    samplesDescriptorBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo noiseDescriptorImageInfo = {};
    noiseDescriptorImageInfo.imageView =
        _textures[_ssaoNoiseTextureID].imageView;
    noiseDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    noiseDescriptorImageInfo.sampler = _ssaoNoiseSampler;

    VkDescriptorImageInfo depthDescriptorImageInfo = {};
    depthDescriptorImageInfo.imageView =
        frameData.vkDepthPassResultShaderReadImageView;
    depthDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthDescriptorImageInfo.sampler = _vkDepthSampler;

    DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                             _descriptorLayoutCache)
        .bindBuffer(1, cameraDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(2, samplesDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImage(3, noiseDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_VERTEX_BIT)
        .bindImage(4, depthDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkSsaoRenderPassDescriptorSet,
               _vkSsaoDescriptorSetLayout);
  }
}

void VulkanRHI::initSsaoSamplesAndNoise() {
  constexpr std::size_t sampleCount = 64;
  constexpr std::size_t sampleBufferSize = sampleCount * sizeof(glm::vec4);
  constexpr std::size_t noiseCount = 16;

  std::random_device randomDevice;
  std::uniform_real_distribution<float> uniformDistribution{0.0001f, 1.0f};

  _ssaoSamplesBuffer = createBuffer(sampleBufferSize,
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_ssaoSamplesBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Ssao samples buffer");

  immediateSubmit(_transferQueueFamilyIndex, [this, &randomDevice,
                                              &uniformDistribution](
                                                 VkCommandBuffer cmd) {
    AllocatedBuffer stagingBuffer =
        createBuffer(sampleBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    void* data;

    VK_CHECK(vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &data));

    glm::vec4* const samples = static_cast<glm::vec4*>(data);

    for (std::size_t i = 0; i < sampleCount; ++i) {
      float scale = static_cast<float>(i) / sampleCount;
      scale = std::lerp(0.1f, 1.0f, scale * scale);
      samples[i] = {scale * (uniformDistribution(randomDevice) * 2.0f - 1.0f),
                    scale * (uniformDistribution(randomDevice) * 2.0 - 1.0f),
                    scale * uniformDistribution(randomDevice), 0.0f};
    }

    vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);
    vmaFlushAllocation(_vmaAllocator, stagingBuffer.allocation, 0,
                       sampleBufferSize);

    VkBufferCopy vkBufferCopy = {};
    vkBufferCopy.srcOffset = 0;
    vkBufferCopy.dstOffset = 0;
    vkBufferCopy.size = sampleBufferSize;

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, _ssaoSamplesBuffer.buffer, 1,
                    &vkBufferCopy);

    vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                     stagingBuffer.allocation);
  });

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _ssaoSamplesBuffer.buffer,
                     _ssaoSamplesBuffer.allocation);
  });

  // Using glm::vec2 because the z is assumed to be 0. The reason is because
  // we are using the noise vector to create otrhonormal basis with normal
  // as the z axis. That orthonormal basis will be used as the tangent space
  // for the fragment when calculating ambient occlusion.
  std::vector<glm::vec2> noiseVectors;
  noiseVectors.reserve(noiseCount);

  for (std::size_t i = 0; i < noiseCount; ++i) {
    noiseVectors.push_back(
        {uniformDistribution(randomDevice), uniformDistribution(randomDevice)});
  }

  rhi::UploadTextureRHI uploadTextureRHI;
  uploadTextureRHI.format = core::TextureFormat::R32G32_SFLOAT;
  uploadTextureRHI.width = 4;
  uploadTextureRHI.height = 4;
  uploadTextureRHI.mipLevels = 1;
  uploadTextureRHI.unpackFunc = [noise = std::move(noiseVectors)](char* dst) {
    std::memcpy(dst, reinterpret_cast<char const*>(noise.data()),
                noise.size() * sizeof(decltype(noiseVectors)::value_type));
  };
  uploadTextureRHI.debugName = "Ssao noise texture";

  (void)noiseVectors;

  _ssaoNoiseTextureID = initTextureResource().id;
  uploadTexture(_ssaoNoiseTextureID, std::move(uploadTextureRHI));

  _deletionQueue.pushFunction(
      [this]() { releaseTexture(_ssaoNoiseTextureID); });

  VkSamplerCreateInfo samplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_REPEAT);

  vkCreateSampler(_vkDevice, &samplerCreateInfo, nullptr, &_ssaoNoiseSampler);

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _ssaoNoiseSampler, nullptr); });
}

void VulkanRHI::initPostProcessingSampler() {
  VkSamplerCreateInfo samplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  VK_CHECK(vkCreateSampler(_vkDevice, &samplerCreateInfo, nullptr,
                           &_postProcessingImageSampler));

  _deletionQueue.pushFunction([this]() {
    vkDestroySampler(_vkDevice, _postProcessingImageSampler, nullptr);
  });
}

void VulkanRHI::initSsaoPostProcessingDescriptors() {
  VkDescriptorImageInfo ssaoResultDescriptorImageInfo = {};
  ssaoResultDescriptorImageInfo.sampler = _postProcessingImageSampler;
  ssaoResultDescriptorImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  for (FrameData& frameData : _frameDataArray) {
    ssaoResultDescriptorImageInfo.imageView =
        frameData.vkSsaoFramebuffer.colorBufferImageView;

    DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                             _descriptorLayoutCache)
        .bindImage(0, ssaoResultDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkSsaoPostProcessingDescriptorSet,
               _vkSsaoPostProcessingDescriptorSetLayout);
  }
}

void VulkanRHI::initPostProcessingQuad() {
  const glm::vec2 quadVertices[6] = {{-1.0f, -1.0f}, {-1.0f, 1.0f},
                                     {1.0f, -1.0f},  {1.0f, -1.0f},
                                     {-1.0f, 1.0f},  {1.0f, 1.0f}};

  _postProcessingQuadBuffer =
      createBuffer(sizeof(quadVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _postProcessingQuadBuffer.buffer,
                     _postProcessingQuadBuffer.allocation);
  });

  void* data;
  vmaMapMemory(_vmaAllocator, _postProcessingQuadBuffer.allocation, &data);

  std::memcpy(data, reinterpret_cast<char const*>(quadVertices),
              sizeof(quadVertices));

  vmaUnmapMemory(_vmaAllocator, _postProcessingQuadBuffer.allocation);
}

void VulkanRHI::initEnvMapRenderPassDataBuffer() {
  _envMapRenderPassDataBuffer = createBuffer(
      getPaddedBufferSize(sizeof(GpuRenderPassData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  setDbgResourceName(_vkDevice,
                     (std::uint64_t)_envMapRenderPassDataBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Env map render pass data");

  GpuRenderPassData renderPassData = {};
  renderPassData.applySsao = VK_FALSE;

  uploadBufferData(0, renderPassData, _envMapRenderPassDataBuffer,
                   VK_QUEUE_FAMILY_IGNORED);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _envMapRenderPassDataBuffer.buffer,
                     _envMapRenderPassDataBuffer.allocation);
  });
}

void VulkanRHI::initEnvMapDataBuffer() {
  _envMapDataBuffer = createBuffer(
      getPaddedBufferSize(sizeof(GpuEnvironmentMapDataCollection)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  setDbgResourceName(_vkDevice, (std::uint64_t)_envMapDataBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Env map data buffer");

  GpuEnvironmentMapDataCollection data = {};
  uploadBufferData(0, data, _envMapDataBuffer, VK_QUEUE_FAMILY_IGNORED);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _envMapDataBuffer.buffer,
                     _envMapDataBuffer.allocation);
  });
}

void VulkanRHI::initImmediateSubmitContext(ImmediateSubmitContext& context,
                                           std::uint32_t queueInd) {
  context.device = _vkDevice;
  context.queueInd = queueInd;

  VkCommandPoolCreateInfo uploadCommandPoolCreateInfo =
      vkinit::commandPoolCreateInfo(queueInd);

  VK_CHECK(vkCreateCommandPool(_vkDevice, &uploadCommandPoolCreateInfo, nullptr,
                               &context.vkCommandPool));

  VkCommandBufferAllocateInfo const vkImmediateSubmitCommandBufferAllocateInfo =
      vkinit::commandBufferAllocateInfo(context.vkCommandPool);

  VK_CHECK(vkAllocateCommandBuffers(_vkDevice,
                                    &vkImmediateSubmitCommandBufferAllocateInfo,
                                    &context.vkCommandBuffer));

  VkFenceCreateInfo immediateSubmitContextFenceCreateInfo =
      vkinit::fenceCreateInfo();

  vkCreateFence(_vkDevice, &immediateSubmitContextFenceCreateInfo, nullptr,
                &context.vkFence);

  context.initialized = true;
}

void VulkanRHI::initTimer() {
  _engineInitTimePoint = Clock::now();

  _timerBuffer = createBuffer(getPaddedBufferSize(sizeof(std::uint32_t)),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  std::uint32_t const msElapsedSinceInit = 0;

  uploadBufferData(0, msElapsedSinceInit, _timerBuffer,
                   VK_QUEUE_FAMILY_IGNORED);

  setDbgResourceName(_vkDevice, (std::uint64_t)_timerBuffer.buffer,
                     VK_OBJECT_TYPE_BUFFER, "Timer buffer");

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _timerBuffer.buffer,
                     _timerBuffer.allocation);
  });
}

void VulkanRHI::updateTimerBuffer(VkCommandBuffer cmd) {
  using namespace std::chrono;

  std::uint32_t const msElapsedSinceInit =
      duration_cast<milliseconds>(Clock::now() - _engineInitTimePoint).count();

  uploadBufferData(0, msElapsedSinceInit, _timerBuffer,
                   _graphicsQueueFamilyIndex, VK_ACCESS_MEMORY_READ_BIT);
}
