#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/renderdoc/renderdoc.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
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
                     rhi::ISurfaceProviderRHI const& surfaceProvider,
                     task::TaskExecutor& taskExecutor) {
  _taskExecutor = &taskExecutor;

  renderdoc::initRenderdoc();

  initVulkan(surfaceProvider);

  _deletionQueue.pushFunction([this]() { destroyUnreferencedResources(); });

  initSwapchain(extent);
  initCommands();
  initMainRenderPass();
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
  initDescriptors();
  initDepthPrepassDescriptors();
  initShadowPassDescriptors();
  initSsaoSamplesAndNoise();
  initSsaoDescriptors();
  initSsaoPostProcessingDescriptors();
  initMainPipelineAndLayouts();
  initDepthPassPipelineLayout();

  waitDeviceIdle();

  IsInitialized = true;

  _vkCmdSetVertexInput = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(
      vkGetDeviceProcAddr(_vkDevice, "vkCmdSetVertexInputEXT"));
}

void VulkanRHI::initResources(rhi::InitResourcesRHI const& initResources) {
  assert(IsInitialized);

  _depthPassShaderId = initShaderResource().id;
  uploadShader(_depthPassShaderId, initResources.shadowPassShader);

  _ssaoShaderId = initShaderResource().id;
  uploadShader(_ssaoShaderId, initResources.ssaoShader);

  _postProcessingShaderId = initShaderResource().id;
  uploadShader(_postProcessingShaderId, initResources.postProcessingShader);

  _deletionQueue.pushFunction([this]() {
    releaseShader(_depthPassShaderId);
    releaseShader(_ssaoShaderId);
    releaseShader(_postProcessingShaderId);
  });

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

  auto const builderReturn =
      builder.set_app_name("Obsidian Engine")
          .request_validation_layers(enable_validation_layers)
          .require_api_version(1, 2, 0)
          .use_default_debug_messenger()
          .build();

  vkb::Instance vkbInstance = builderReturn.value();

  _vkInstance = vkbInstance.instance;
  _vkDebugMessenger = vkbInstance.debug_messenger;

  surfaceProvider.provideSurface(*this);

  VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures = {};
  vkPhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

  vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice =
      vkbSelector.set_minimum_version(1, 2)
          .set_surface(_vkSurface)
          .add_required_extension(
              VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME)
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

  _transferQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
  _gpuQueues[_transferQueueFamilyIndex] =
      vkbDevice.get_queue(vkb::QueueType::transfer).value();

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

void VulkanRHI::initMainRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .addColorAttachment(_vkbSwapchain.image_format,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
      .addDepthAttachment(_depthFormat, false)
      .addColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .addDepthSubpassReference(0,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .build(_mainRenderPass);

  _swapchainDeletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _mainRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initDepthRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .addDepthAttachment(_depthFormat)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .addDepthSubpassReference(
          0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .build(_depthRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _depthRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initSsaoRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .addColorAttachment(_ssaoFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .addDepthAttachment(_depthFormat, false)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .addColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .addDepthSubpassReference(
          0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .build(_ssaoRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _ssaoRenderPass.vkRenderPass, nullptr);
  });
}

void VulkanRHI::initPostProcessingRenderPass() {
  RenderPassBuilder::begin(_vkDevice)
      .addColorAttachment(_ssaoFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .setSubpassPipelineBindPoint(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
      .addColorSubpassReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .build(_postProcessingRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _postProcessingRenderPass.vkRenderPass,
                        nullptr);
  });
}

void VulkanRHI::initSwapchainFramebuffers() {
  _vkSwapchainFramebuffers.resize(_vkbSwapchain.image_count);

  for (int i = 0; i < _vkbSwapchain.image_count; ++i) {
    for (int j = 0; j < frameOverlap; ++j) {
      _vkSwapchainFramebuffers[i][j] = _mainRenderPass.generateFramebuffer(
          _vmaAllocator, _vkbSwapchain.extent, {}, _swapchainImageViews[i],
          _frameDataArray[j].vkDepthPrepassFramebuffer.depthBufferImageView);
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

    _swapchainDeletionQueue.pushFunction(
        [this, &framebuffer = frameData.vkDepthPrepassFramebuffer]() {
          vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
          vkDestroyImageView(_vkDevice, framebuffer.depthBufferImageView,
                             nullptr);
          vmaDestroyImage(_vmaAllocator, framebuffer.depthBufferImage.vkImage,
                          framebuffer.depthBufferImage.allocation);
        });
  }

  VkImageCreateInfo depthPrepassResultImgCreateInfo = vkinit::imageCreateInfo(
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      {_vkbSwapchain.extent.width, _vkbSwapchain.extent.height, 1},
      _depthFormat);
  depthPrepassResultImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo depthPrepassResultAllocationCreateInfo = {};
  depthPrepassResultAllocationCreateInfo.usage =
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateImage(_vmaAllocator, &depthPrepassResultImgCreateInfo,
                          &depthPrepassResultAllocationCreateInfo,
                          &_depthPassResultShaderReadImage.vkImage,
                          &_depthPassResultShaderReadImage.allocation,
                          nullptr));

  VkImageViewCreateInfo const depthPrepassResultImgViewCreateInfo =
      vkinit::imageViewCreateInfo(_depthPassResultShaderReadImage.vkImage,
                                  _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(_vkDevice, &depthPrepassResultImgViewCreateInfo,
                             nullptr, &_depthPassResultShaderReadImageView));

  _swapchainDeletionQueue.pushFunction([this]() {
    vkDestroyImageView(_vkDevice, _depthPassResultShaderReadImageView, nullptr);
    vmaDestroyImage(_vmaAllocator, _depthPassResultShaderReadImage.vkImage,
                    _depthPassResultShaderReadImage.allocation);
  });
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

void VulkanRHI::initSsaoFramebuffers() {
  for (FrameData& frameData : _frameDataArray) {
    frameData.vkSsaoFramebuffer = _ssaoRenderPass.generateFramebuffer(
        _vmaAllocator, _vkbSwapchain.extent,
        {
            .colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
        },
        VK_NULL_HANDLE,
        frameData.vkDepthPrepassFramebuffer.depthBufferImageView);

    _swapchainDeletionQueue.pushFunction(
        [this, &framebuffer = frameData.vkSsaoFramebuffer]() {
          vkDestroyFramebuffer(_vkDevice, framebuffer.vkFramebuffer, nullptr);
          vkDestroyImageView(_vkDevice, framebuffer.colorBufferImageView,
                             nullptr);
          vmaDestroyImage(_vmaAllocator, framebuffer.colorBufferImage.vkImage,
                          framebuffer.colorBufferImage.allocation);
        });
  }
}

void VulkanRHI::initSsaoPostProcessingFramebuffers() {
  for (FrameData& frameData : _frameDataArray) {
    frameData.vkSsaoPostProcessingFramebuffer =
        _postProcessingRenderPass.generateFramebuffer(
            _vmaAllocator, _vkbSwapchain.extent,
            {.colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT});

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

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, false);

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
      _vkUnlitTexturedMaterialDescriptorSetLayout, _vkEmptyDescriptorSetLayout};

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
      _vkEmptyDescriptorSetLayout,
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
  vkPushConstantRange.size = sizeof(MeshPushConstants);
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

  VkShaderModule const shaderModule =
      _shaderModules[_depthPassShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule));

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule));

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

  VkShaderModule const shaderModule =
      _shaderModules[_depthPassShaderId].vkShaderModule;
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule));

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule));

  pipelineBuilder._vkPipelineLayout = _vkDepthPipelineLayout;

  _vkDepthPrepassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _depthRenderPass.vkRenderPass);

  _deletionQueue.pushFunction([this] {
    vkDestroyPipeline(_vkDevice, _vkDepthPrepassPipeline, nullptr);
  });
}

void VulkanRHI::initSsaoPipeline() {
  PipelineBuilder pipelineBuilder = {};

  VkShaderModule const ssaoShaderModule =
      _shaderModules[_ssaoShaderId].vkShaderModule;

  pipelineBuilder._vkShaderStageCreateInfos.reserve(2);
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            ssaoShaderModule));
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            ssaoShaderModule));
  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(true, false);
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
  pushConstantRange.size = getPaddedBufferSize(sizeof(GPUObjectData));

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
          _shaderModules[_postProcessingShaderId].vkShaderModule));
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_FRAGMENT_BIT,
          _shaderModules[_postProcessingShaderId].vkShaderModule));

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
  pushConstantRange.size =
      getPaddedBufferSize(sizeof(PostProcessingpushConstants));

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

void VulkanRHI::initDescriptors() {
  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .build(_emptyDescriptorSet, _vkEmptyDescriptorSetLayout);

  std::size_t const paddedSceneDataSize =
      getPaddedBufferSize(sizeof(GPUSceneData));
  std::size_t const sceneDataBufferSize = frameOverlap * paddedSceneDataSize;
  _sceneDataBuffer =
      createBuffer(sceneDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _swapchainDeletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _sceneDataBuffer.buffer,
                     _sceneDataBuffer.allocation);
  });

  _cameraBuffer =
      createBuffer(frameOverlap * getPaddedBufferSize(sizeof(GPUCameraData)),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  _swapchainDeletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _cameraBuffer.buffer,
                     _cameraBuffer.allocation);
    _cameraBuffer = {};
  });

  VkDescriptorBufferInfo cameraDescriptorBufferInfo;
  cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
  cameraDescriptorBufferInfo.offset = 0;
  cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

  VkDescriptorBufferInfo sceneDescriptorBufferInfo;
  sceneDescriptorBufferInfo.buffer = _sceneDataBuffer.buffer;
  sceneDescriptorBufferInfo.offset = 0;
  sceneDescriptorBufferInfo.range = sizeof(GPUSceneData);

  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, cameraDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
      .bindBuffer(1, sceneDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(_vkGlobalDescriptorSet, _vkGlobalDescriptorSetLayout);

  VkSamplerCreateInfo const vkDepthSamplerCreateInfo =
      vkinit::samplerCreateInfo(VK_FILTER_LINEAR,
                                VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  VK_CHECK(vkCreateSampler(_vkDevice, &vkDepthSamplerCreateInfo, nullptr,
                           &_vkDepthSampler));

  _swapchainDeletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _vkDepthSampler, nullptr); });

  _lightDataBuffer = createBuffer(
      frameOverlap * getPaddedBufferSize(sizeof(GPULightData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _swapchainDeletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _lightDataBuffer.buffer,
                     _lightDataBuffer.allocation);
  });

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

    VkDescriptorBufferInfo vkLightDataBufferInfo = {};
    vkLightDataBufferInfo.buffer = _lightDataBuffer.buffer;
    vkLightDataBufferInfo.offset = 0;
    vkLightDataBufferInfo.range = sizeof(GPULightData);

    VkDescriptorImageInfo vkSsaoImageInfo = {};
    vkSsaoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkSsaoImageInfo.sampler = _vkLinearRepeatSampler;
    vkSsaoImageInfo.imageView =
        frameData.vkSsaoPostProcessingFramebuffer.colorBufferImageView;

    VkDescriptorImageInfo vkDepthPrepassimageInfo = {};
    vkDepthPrepassimageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkDepthPrepassimageInfo.imageView =
        frameData.vkDepthPrepassFramebuffer.depthBufferImageView;
    vkDepthPrepassimageInfo.sampler = _vkDepthSampler;

    DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                             _descriptorLayoutCache)
        .bindImages(0, vkShadowMapDescriptorImageInfos,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(1, vkLightDataBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImage(2, vkSsaoImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkMainRenderPassDescriptorSet,
               _vkMainRenderPassDescriptorSetLayout);
  }

  {
    std::array<VkDescriptorSetLayoutBinding, 3> litMaterialBinding = {};
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

    std::array<VkDescriptorBindingFlags, 3> bindingFlags = {
        0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
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
    std::array<VkDescriptorSetLayoutBinding, 2> unlitMaterialBinding = {};

    unlitMaterialBinding[0].binding = 0;
    unlitMaterialBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    unlitMaterialBinding[0].descriptorCount = 1;
    unlitMaterialBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    unlitMaterialBinding[1].binding = 1;
    unlitMaterialBinding[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    unlitMaterialBinding[1].descriptorCount = 1;
    unlitMaterialBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
        0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

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
}

void VulkanRHI::initDepthPrepassDescriptors() {
  VkDescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = _cameraBuffer.buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = getPaddedBufferSize(sizeof(GPUCameraData));

  DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_depthPrepassDescriptorSet, _vkDepthPassDescriptorSetLayout);
}

void VulkanRHI::initShadowPassDescriptors() {
  _shadowPassCameraBuffer =
      createBuffer(frameOverlap * rhi::maxLightsPerDrawPass *
                       getPaddedBufferSize(sizeof(GPUCameraData)),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

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
      .bindBuffer(0, vkCameraDatabufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_vkShadowPassDescriptorSet);
}

void VulkanRHI::initSsaoDescriptors() {
  for (FrameData& frameData : _frameDataArray) {
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
    depthDescriptorImageInfo.imageView = _depthPassResultShaderReadImageView;
    depthDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthDescriptorImageInfo.sampler = _vkDepthSampler;

    DescriptorBuilder::begin(_vkDevice, _swapchainBoundDescriptorAllocator,
                             _descriptorLayoutCache)
        .bindBuffer(0, samplesDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImage(1, noiseDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindImage(2, depthDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkSsaoRenderPassDescriptorSet,
               _vkSsaoDescriptorSetLayout);
  }
}

void VulkanRHI::initSsaoSamplesAndNoise() {
  constexpr const std::size_t sampleCount = 64;
  constexpr const std::size_t noiseCount = 16;

  _ssaoSamplesBuffer = createBuffer(
      sampleCount * sizeof(glm::vec4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* data;

  VK_CHECK(vmaMapMemory(_vmaAllocator, _ssaoSamplesBuffer.allocation, &data));

  std::random_device randomDevice;
  std::uniform_real_distribution<float> uniformDistribution{0.0001f, 1.0f};

  glm::vec4* const samples = static_cast<glm::vec4*>(data);

  for (std::size_t i = 0; i < sampleCount; ++i) {
    float scale = static_cast<float>(i) / sampleCount;
    scale = std::lerp(0.1f, 1.0f, scale * scale);
    samples[i] = {scale * (uniformDistribution(randomDevice) * 2.0f - 1.0f),
                  scale * (uniformDistribution(randomDevice) * 2.0 - 1.0f),
                  scale * uniformDistribution(randomDevice), 0.0f};
  }

  vmaUnmapMemory(_vmaAllocator, _ssaoSamplesBuffer.allocation);

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
