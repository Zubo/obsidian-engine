#include "glm/ext/vector_float3.hpp"
#include "obsidian/rhi/resource_rhi.hpp"
#include "obsidian/vk_rhi/vk_pipeline_builder.hpp"
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/renderdoc/renderdoc.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_mesh.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>
#include <obsidian/vk_rhi/vk_types.hpp>

#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <random>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <numeric>

using namespace obsidian::vk_rhi;

void VulkanRHI::init(rhi::WindowExtentRHI extent,
                     rhi::ISurfaceProviderRHI const& surfaceProvider) {
  _windowExtent.height = extent.height;
  _windowExtent.width = extent.width;

  renderdoc::initRenderdoc();

  initVulkan(surfaceProvider);

  initSwapchain();

  initCommands();

  initDefaultRenderPass();

  initDepthRenderPass();

  initSsaoRenderPass();

  initFramebuffers();

  initShadowPassFramebuffers();

  initDepthPrepassFramebuffers();

  initSyncStructures();

  initDescriptors();

  initDepthPrepassDescriptors();

  initShadowPassDescriptors();

  initSSAOSamplesAndNoise();

  initSsaoDescriptors();

  initDefaultPipelineAndLayouts();

  initDepthPassPipelineLayout();

  IsInitialized = true;
}

void VulkanRHI::initResources(rhi::InitResourcesRHI const& initResources) {
  assert(IsInitialized);

  _depthPassShaderId = uploadShader(initResources.shadowPassShader);
  _ssaoShaderId = uploadShader(initResources.ssaoShader);

  _deletionQueue.pushFunction([this]() {
    vkDestroyShaderModule(_vkDevice, _shaderModules[_depthPassShaderId],
                          nullptr);
    vkDestroyShaderModule(_vkDevice, _shaderModules[_ssaoShaderId], nullptr);
  });

  initDepthPrepassPipeline();
  initShadowPassPipeline();
  initSsaoPipeline();
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

  vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector.set_minimum_version(1, 2)
                                              .set_surface(_vkSurface)
                                              .select()
                                              .value();

  vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
  VkPhysicalDeviceVulkan11Features vkDeviceFeatures = {};
  vkDeviceFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vkDeviceFeatures.pNext = nullptr;
  vkDeviceFeatures.shaderDrawParameters = VK_TRUE;
  vkbDeviceBuilder.add_pNext(&vkDeviceFeatures);
  vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

  _vkDevice = vkbDevice.device;
  _vkPhysicalDevice = vkbPhysicalDevice.physical_device;
  _vkPhysicalDeviceProperties = vkbPhysicalDevice.properties;

  _vkGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _vkPhysicalDevice;
  allocatorInfo.device = _vkDevice;
  allocatorInfo.instance = _vkInstance;
  VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_vmaAllocator));

  _deletionQueue.pushFunction([this] { vmaDestroyAllocator(_vmaAllocator); });
}

void VulkanRHI::initSwapchain(VkSwapchainKHR oldSwapchain) {
  vkb::SwapchainBuilder swapchainBuilder{_vkPhysicalDevice, _vkDevice,
                                         _vkSurface};
  swapchainBuilder.use_default_format_selection()
      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
      .set_desired_extent(_windowExtent.width, _windowExtent.height);

  if (_vkSwapchain != VK_NULL_HANDLE) {
    swapchainBuilder.set_old_swapchain(oldSwapchain);
  }

  vkb::Swapchain vkbSwapchain = swapchainBuilder.build().value();

  _vkSwapchain = vkbSwapchain.swapchain;
  _vkSwapchainImages = vkbSwapchain.get_images().value();
  std::vector<VkImageView> swapchainColorImageViews =
      vkbSwapchain.get_image_views().value();

  std::size_t const swapchainSize = swapchainColorImageViews.size();

  _vkFramebufferImageViews.resize(swapchainSize);

  _vkSwapchainImageFormat = vkbSwapchain.image_format;

  _swapchainDeletionQueue.pushFunction([this, swapchainColorImageViews]() {
    for (VkImageView const& vkSwapchainImgView : swapchainColorImageViews) {
      vkDestroyImageView(_vkDevice, vkSwapchainImgView, nullptr);
    }
  });

  if (oldSwapchain == VK_NULL_HANDLE) {
    _deletionQueue.pushFunction(
        [this]() { vkDestroySwapchainKHR(_vkDevice, _vkSwapchain, nullptr); });
  } else {
    vkDestroySwapchainKHR(_vkDevice, oldSwapchain, nullptr);
  }

  _vkFramebuffers.resize(swapchainSize);

  createDepthImage(_depthBufferAttachmentImage,
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkImageViewCreateInfo depthImageViewCreateInfo =
      vkinit::imageViewCreateInfo(_depthBufferAttachmentImage.vkImage,
                                  _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

  for (int i = 0; i < swapchainSize; ++i) {
    FramebufferImageViews& swapchainImageViews = _vkFramebufferImageViews.at(i);
    std::vector<VkImageView>& imageViews = swapchainImageViews.vkImageViews;

    imageViews.push_back(swapchainColorImageViews[i]);
    VkImageView& depthImageView =
        swapchainImageViews.vkImageViews.emplace_back();

    VK_CHECK(vkCreateImageView(_vkDevice, &depthImageViewCreateInfo, nullptr,
                               &depthImageView));

    _swapchainDeletionQueue.pushFunction([this, depthImageView]() {
      vkDestroyImageView(_vkDevice, depthImageView, nullptr);
    });
  }
  _swapchainDeletionQueue.pushFunction([this]() {
    _vkFramebufferImageViews.clear();
    _vkFramebuffers.clear();
    vmaDestroyImage(_vmaAllocator, _depthBufferAttachmentImage.vkImage,
                    _depthBufferAttachmentImage.allocation);
  });
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

  VkCommandPoolCreateInfo uploadCommandPoolCreateInfo =
      vkinit::commandPoolCreateInfo(_graphicsQueueFamilyIndex);

  VK_CHECK(vkCreateCommandPool(_vkDevice, &uploadCommandPoolCreateInfo, nullptr,
                               &_immediateSubmitContext.vkCommandPool));
  _deletionQueue.pushFunction([this]() {
    vkDestroyCommandPool(_vkDevice, _immediateSubmitContext.vkCommandPool,
                         nullptr);
  });

  VkCommandBufferAllocateInfo const vkImmediateSubmitCommandBufferAllocateInfo =
      vkinit::commandBufferAllocateInfo(_immediateSubmitContext.vkCommandPool);

  VK_CHECK(vkAllocateCommandBuffers(_vkDevice,
                                    &vkImmediateSubmitCommandBufferAllocateInfo,
                                    &_immediateSubmitContext.vkCommandBuffer));
}

void VulkanRHI::initDefaultRenderPass() {
  VkAttachmentDescription vkAttachments[2] = {};

  VkAttachmentDescription& vkColorAttachment = vkAttachments[0];
  vkColorAttachment.format = _vkSwapchainImageFormat;
  vkColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  vkColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  vkColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  vkColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  vkColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  vkColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference vkColorAttachmentReference = {};
  vkColorAttachmentReference.attachment = 0;
  vkColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription& vkDepthAttachment = vkAttachments[1];
  vkDepthAttachment.format = _depthFormat;
  vkDepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  vkDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  vkDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  vkDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  vkDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  vkDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkDepthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference vkDepthAttachmentReference = {};
  vkDepthAttachmentReference.attachment = 1;
  vkDepthAttachmentReference.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription vkSubpassDescription = {};
  vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkSubpassDescription.colorAttachmentCount = 1;
  vkSubpassDescription.pColorAttachments = &vkColorAttachmentReference;
  vkSubpassDescription.pDepthStencilAttachment = &vkDepthAttachmentReference;

  VkRenderPassCreateInfo vkRenderPassCreateInfo = {};
  vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  vkRenderPassCreateInfo.pNext = nullptr;

  vkRenderPassCreateInfo.attachmentCount = 2;
  vkRenderPassCreateInfo.pAttachments = vkAttachments;

  vkRenderPassCreateInfo.subpassCount = 1;
  vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;

  VK_CHECK(vkCreateRenderPass(_vkDevice, &vkRenderPassCreateInfo, nullptr,
                              &_vkDefaultRenderPass));

  _swapchainDeletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _vkDefaultRenderPass, nullptr);
  });
}

void VulkanRHI::initDepthRenderPass() {
  VkRenderPassCreateInfo vkRenderPassCreateInfo = {};
  vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  vkRenderPassCreateInfo.pNext = nullptr;

  VkAttachmentDescription vkAttachmentDescription = {};
  vkAttachmentDescription.format = _depthFormat;
  vkAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  vkAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  vkAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  vkAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  vkAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  vkAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkAttachmentDescription.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  vkRenderPassCreateInfo.pAttachments = &vkAttachmentDescription;
  vkRenderPassCreateInfo.attachmentCount = 1;

  VkSubpassDescription vkSubpassDescription = {};
  vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkSubpassDescription.inputAttachmentCount = 0;
  vkSubpassDescription.colorAttachmentCount = 0;

  VkAttachmentReference vkDepthStencilAttachmentReference = {};
  vkDepthStencilAttachmentReference.attachment = 0;
  vkDepthStencilAttachmentReference.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  vkSubpassDescription.pDepthStencilAttachment =
      &vkDepthStencilAttachmentReference;

  vkRenderPassCreateInfo.subpassCount = 1;
  vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;

  VK_CHECK(vkCreateRenderPass(_vkDevice, &vkRenderPassCreateInfo, nullptr,
                              &_vkDepthRenderPass));

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _vkDepthRenderPass, nullptr);
  });
}

void VulkanRHI::initSsaoRenderPass() {
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.pNext = nullptr;

  std::array<VkAttachmentDescription, 2> attachmentDescriptions;
  VkAttachmentDescription& colorAttachmentDescr = attachmentDescriptions[0];
  colorAttachmentDescr.format = VK_FORMAT_R32_SFLOAT;
  colorAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depthAttachmentDescr = attachmentDescriptions[1];
  depthAttachmentDescr.format = _depthFormat;
  depthAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachmentDescr.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  renderPassCreateInfo.attachmentCount = 2;
  renderPassCreateInfo.pAttachments = attachmentDescriptions.data();

  VkAttachmentReference colorAttachmentReference;
  colorAttachmentReference.attachment = 0;
  colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentreference;
  depthStencilAttachmentreference.attachment = 1;
  depthStencilAttachmentreference.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorAttachmentReference;
  subpassDescription.pDepthStencilAttachment = &depthStencilAttachmentreference;

  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDescription;

  VK_CHECK(vkCreateRenderPass(_vkDevice, &renderPassCreateInfo, nullptr,
                              &_vkSsaoRenderPass));

  _deletionQueue.pushFunction(
      [this]() { vkDestroyRenderPass(_vkDevice, _vkSsaoRenderPass, nullptr); });
}

void VulkanRHI::initFramebuffers() {
  std::size_t const swapchainImageCount = _vkFramebufferImageViews.size();
  VkFramebufferCreateInfo vkFramebufferCreateInfo = {};
  vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  vkFramebufferCreateInfo.pNext = nullptr;
  vkFramebufferCreateInfo.renderPass = _vkDefaultRenderPass;
  vkFramebufferCreateInfo.width = _windowExtent.width;
  vkFramebufferCreateInfo.height = _windowExtent.height;
  vkFramebufferCreateInfo.layers = 1;

  for (int i = 0; i < swapchainImageCount; ++i) {
    FramebufferImageViews& framebufferImageViews =
        _vkFramebufferImageViews.at(i);
    vkFramebufferCreateInfo.attachmentCount =
        framebufferImageViews.vkImageViews.size();
    vkFramebufferCreateInfo.pAttachments =
        framebufferImageViews.vkImageViews.data();

    VK_CHECK(vkCreateFramebuffer(_vkDevice, &vkFramebufferCreateInfo, nullptr,
                                 &_vkFramebuffers[i]));

    _swapchainDeletionQueue.pushFunction([this, i]() {
      vkDestroyFramebuffer(_vkDevice, _vkFramebuffers[i], nullptr);
    });
  }
}

void VulkanRHI::initDepthPrepassFramebuffers() {
  for (FrameData& frameData : _frameDataArray) {
    createDepthImage(frameData.depthPrepassImage,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT);
    _swapchainDeletionQueue.pushFunction([this, &frameData]() {
      vmaDestroyImage(_vmaAllocator, frameData.depthPrepassImage.vkImage,
                      frameData.depthPrepassImage.allocation);
    });

    VkImageViewCreateInfo depthPassImageViewCreateInfo = {};
    depthPassImageViewCreateInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthPassImageViewCreateInfo.pNext = nullptr;

    depthPassImageViewCreateInfo.image = frameData.depthPrepassImage.vkImage;
    depthPassImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthPassImageViewCreateInfo.format = _depthFormat;
    depthPassImageViewCreateInfo.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT;
    depthPassImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthPassImageViewCreateInfo.subresourceRange.levelCount = 1;
    depthPassImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthPassImageViewCreateInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(_vkDevice, &depthPassImageViewCreateInfo, nullptr,
                      &frameData.vkDepthPrepassImageView);

    _swapchainDeletionQueue.pushFunction([this, &frameData]() {
      vkDestroyImageView(_vkDevice, frameData.vkDepthPrepassImageView, nullptr);
    });

    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = nullptr;

    framebufferCreateInfo.flags = 0;
    framebufferCreateInfo.renderPass = _vkDepthRenderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = &frameData.vkDepthPrepassImageView;
    framebufferCreateInfo.width = _windowExtent.width;
    framebufferCreateInfo.height = _windowExtent.height;
    framebufferCreateInfo.layers = 1;

    vkCreateFramebuffer(_vkDevice, &framebufferCreateInfo, nullptr,
                        &frameData.vkDepthPrepassFramebuffer);

    _swapchainDeletionQueue.pushFunction([this, &frameData]() {
      vkDestroyFramebuffer(_vkDevice, frameData.vkDepthPrepassFramebuffer,
                           nullptr);
    });
  }
}

void VulkanRHI::initShadowPassFramebuffers() {
  for (std::size_t i = 0; i < _frameDataArray.size(); ++i) {
    for (std::size_t j = 0; j < rhi::maxLightsPerDrawPass; ++j) {
      AllocatedImage& imageShadowPassAttachment =
          _frameDataArray[i].shadowMapImages[j];

      VkExtent3D vkShadowPassAttachmentExtent = {shadowPassAttachmentWidth,
                                                 shadowPassAttachmentHeight, 1};
      VkImageCreateInfo const vkImageShadowPassAttachmentCreateInfo =
          vkinit::imageCreateInfo(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT,
                                  vkShadowPassAttachmentExtent, _depthFormat);

      VmaAllocationCreateInfo allocationCreateInfo = {};
      allocationCreateInfo.flags = 0;
      allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
      allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      VK_CHECK(vmaCreateImage(
          _vmaAllocator, &vkImageShadowPassAttachmentCreateInfo,
          &allocationCreateInfo, &imageShadowPassAttachment.vkImage,
          &imageShadowPassAttachment.allocation, nullptr));

      _deletionQueue.pushFunction([this, imageShadowPassAttachment]() {
        vmaDestroyImage(_vmaAllocator, imageShadowPassAttachment.vkImage,
                        imageShadowPassAttachment.allocation);
      });

      VkImageViewCreateInfo shadowMapImageViewCreateInfo =
          vkinit::imageViewCreateInfo(imageShadowPassAttachment.vkImage,
                                      _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
      VkImageView& vkShadowMapImageView =
          _frameDataArray[i].shadowMapImageViews[j];
      VK_CHECK(vkCreateImageView(_vkDevice, &shadowMapImageViewCreateInfo,
                                 nullptr, &vkShadowMapImageView));

      _deletionQueue.pushFunction([this, vkShadowMapImageView]() {
        vkDestroyImageView(_vkDevice, vkShadowMapImageView, nullptr);
      });

      VkFramebufferCreateInfo vkFramebufferCreateInfo = {};

      vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      vkFramebufferCreateInfo.pNext = nullptr;

      vkFramebufferCreateInfo.renderPass = _vkDepthRenderPass;
      vkFramebufferCreateInfo.attachmentCount = 1;
      vkFramebufferCreateInfo.pAttachments = &vkShadowMapImageView;
      vkFramebufferCreateInfo.width = shadowPassAttachmentWidth;
      vkFramebufferCreateInfo.height = shadowPassAttachmentHeight;
      vkFramebufferCreateInfo.layers = 1;

      VK_CHECK(vkCreateFramebuffer(_vkDevice, &vkFramebufferCreateInfo, nullptr,
                                   &_frameDataArray[i].shadowFrameBuffers[j]));

      _deletionQueue.pushFunction(
          [this, fb = _frameDataArray[i].shadowFrameBuffers[j]]() {
            vkDestroyFramebuffer(_vkDevice, fb, nullptr);
          });
    }
  }
}

void VulkanRHI::initSsaoFramebuffers() {
  VkExtent3D const extent{_windowExtent.width, _windowExtent.height, 1};
  VkImageCreateInfo const colorImageCreateInfo = vkinit::imageCreateInfo(
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, extent, VK_FORMAT_R32_SFLOAT);
  VkImageViewCreateInfo colorImageViewCreateInfo = vkinit::imageViewCreateInfo(
      VK_NULL_HANDLE, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

  VkImageCreateInfo const depthImageCreateInfo = vkinit::imageCreateInfo(
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, extent, _depthFormat);
  VkImageViewCreateInfo depthImageViewCreateInfo = vkinit::imageViewCreateInfo(
      VK_NULL_HANDLE, _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

  VmaAllocationCreateInfo allocationCreateInfo = {};
  allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkFramebufferCreateInfo frameBufferCreateInfo;
  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameBufferCreateInfo.pNext = nullptr;
  frameBufferCreateInfo.renderPass = _vkSsaoRenderPass;
  frameBufferCreateInfo.attachmentCount = 2;

  for (FrameData& frameData : _frameDataArray) {
    VK_CHECK(vmaCreateImage(_vmaAllocator, &colorImageCreateInfo,
                            &allocationCreateInfo,
                            &frameData.ssaoPassImage.vkImage,
                            &frameData.ssaoPassImage.allocation, nullptr));

    _swapchainDeletionQueue.pushFunction([this, &frameData] {
      vmaDestroyImage(_vmaAllocator, frameData.ssaoPassImage.vkImage,
                      frameData.ssaoPassImage.allocation);
    });

    colorImageViewCreateInfo.image = frameData.ssaoPassImage.vkImage;
    VkImageView& colorAttachmentImageView =
        frameData.ssaoFramebufferImageViews[0];
    vkCreateImageView(_vkDevice, &colorImageViewCreateInfo, nullptr,
                      &colorAttachmentImageView);

    VK_CHECK(vmaCreateImage(_vmaAllocator, &depthImageCreateInfo,
                            &allocationCreateInfo,
                            &frameData.ssaoPassImage.vkImage,
                            &frameData.ssaoPassImage.allocation, nullptr));

    depthImageViewCreateInfo.image = frameData.ssaoPassImage.vkImage;

    vkCreateImageView(_vkDevice, &depthImageViewCreateInfo, nullptr,
                      &frameData.ssaoFramebufferImageViews[1]);

    _swapchainDeletionQueue.pushFunction([this, &frameData]() {
      vkDestroyImageView(_vkDevice, frameData.ssaoFramebufferImageViews[0],
                         nullptr);
      vkDestroyImageView(_vkDevice, frameData.ssaoFramebufferImageViews[1],
                         nullptr);
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

  VkFenceCreateInfo immediateSubmitContextFenceCreateInfo =
      vkinit::fenceCreateInfo();

  vkCreateFence(_vkDevice, &immediateSubmitContextFenceCreateInfo, nullptr,
                &_immediateSubmitContext.vkFence);

  _deletionQueue.pushFunction([this]() {
    vkDestroyFence(_vkDevice, _immediateSubmitContext.vkFence, nullptr);
  });
}

void VulkanRHI::initDefaultPipelineAndLayouts() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(VK_TRUE);

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);
  pipelineBuilder._vkColorBlendAttachmentState =
      vkinit::colorBlendAttachmentState();
  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  pipelineBuilder._vertexInputAttributeDescription =
      Vertex::getVertexInputDescription();

  VkPipelineLayoutCreateInfo meshPipelineLayoutInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> const meshDescriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkLitMeshRenderPassDescriptorSetLayout,
      _vkTexturedMaterialDescriptorSetLayout, _vkObjectDataDescriptorSetLayout};

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
      _vkLitMeshRenderPassDescriptorSetLayout,
      _vkTexturedMaterialDescriptorSetLayout,
      _vkObjectDataDescriptorSetLayout,
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
      _vkDepthPassGlobalDescriptorSetLayout, _vkEmptyDescriptorSetLayout,
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
      vkinit::depthStencilStateCreateInfo(VK_TRUE);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);

  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  pipelineBuilder._vertexInputAttributeDescription =
      Vertex::getVertexInputDescription(true, false, false, false);

  pipelineBuilder._vkViewport.x = 0.f;
  pipelineBuilder._vkViewport.y = 0.f;
  pipelineBuilder._vkViewport.width = shadowPassAttachmentWidth;
  pipelineBuilder._vkViewport.height = shadowPassAttachmentHeight;
  pipelineBuilder._vkViewport.minDepth = 0.0f;
  pipelineBuilder._vkViewport.maxDepth = 1.0f;

  pipelineBuilder._vkScissor.offset = {0, 0};
  pipelineBuilder._vkScissor.extent = {shadowPassAttachmentWidth,
                                       shadowPassAttachmentHeight};

  VertexInputDescription shadowPassVertexInputDescription =
      Vertex::getVertexInputDescription(true, false, false, false);

  VkShaderModule const shaderModule = _shaderModules[_depthPassShaderId];
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule));

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule));

  pipelineBuilder._vkPipelineLayout = _vkDepthPipelineLayout;

  _vkShadowPassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _vkDepthRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipeline(_vkDevice, _vkShadowPassPipeline, nullptr);
  });
}

void VulkanRHI::initDepthPrepassPipeline() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(VK_TRUE);

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);

  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  pipelineBuilder._vertexInputAttributeDescription =
      Vertex::getVertexInputDescription(true, false, false, false);

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

  VkShaderModule const shaderModule = _shaderModules[_depthPassShaderId];
  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shaderModule));

  pipelineBuilder._vkShaderStageCreateInfos.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shaderModule));

  pipelineBuilder._vkPipelineLayout = _vkDepthPipelineLayout;

  _vkDepthPrepassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _vkDepthRenderPass);

  _deletionQueue.pushFunction([this] {
    vkDestroyPipeline(_vkDevice, _vkDepthPrepassPipeline, nullptr);
  });
}

void VulkanRHI::initSsaoPipeline() {
  PipelineBuilder pipelineBuilder = {};

  VkShaderModule const ssaoShaderModule = _shaderModules[_ssaoShaderId];

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
      vkinit::depthStencilStateCreateInfo(true);
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
  pipelineBuilder._vertexInputAttributeDescription =
      Vertex::getVertexInputDescription(true, true, false, true);

  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  pipelineBuilder._vkDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

  _vkSsaoPipeline = pipelineBuilder.buildPipeline(_vkDevice, _vkSsaoRenderPass);

  _deletionQueue.pushFunction(
      [this]() { vkDestroyPipeline(_vkDevice, _vkSsaoPipeline, nullptr); });
}

void VulkanRHI::initDescriptors() {
  _descriptorLayoutCache.init(_vkDevice);
  _descriptorAllocator.init(_vkDevice);

  _deletionQueue.pushFunction([this]() {
    _descriptorAllocator.cleanup();
    _descriptorLayoutCache.cleanup();
  });

  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .build(_emptyDescriptorSet, _vkEmptyDescriptorSetLayout);

  std::size_t const paddedSceneDataSize =
      getPaddedBufferSize(sizeof(GPUSceneData));
  std::size_t const sceneDataBufferSize = frameOverlap * paddedSceneDataSize;
  _sceneDataBuffer =
      createBuffer(sceneDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _sceneDataBuffer.buffer,
                     _sceneDataBuffer.allocation);
  });

  _cameraBuffer =
      createBuffer(frameOverlap * getPaddedBufferSize(sizeof(GPUCameraData)),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  _deletionQueue.pushFunction([this]() {
    AllocatedBuffer const& buffer = _cameraBuffer;
    vmaDestroyBuffer(_vmaAllocator, buffer.buffer, buffer.allocation);
  });

  _shadowPassCameraBuffer =
      createBuffer(frameOverlap * rhi::maxLightsPerDrawPass *
                       getPaddedBufferSize(sizeof(GPUCameraData)),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _shadowPassCameraBuffer.buffer,
                     _shadowPassCameraBuffer.allocation);
  });

  VkDescriptorBufferInfo cameraDescriptorBufferInfo;
  cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
  cameraDescriptorBufferInfo.offset = 0;
  cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

  VkDescriptorBufferInfo sceneDescriptorBufferInfo;
  sceneDescriptorBufferInfo.buffer = _sceneDataBuffer.buffer;
  sceneDescriptorBufferInfo.offset = 0;
  sceneDescriptorBufferInfo.range = sizeof(GPUSceneData);

  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, cameraDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
      .bindBuffer(1, sceneDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(_vkGlobalDescriptorSet, _vkGlobalDescriptorSetLayout);

  VkSamplerCreateInfo vkSamplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  VK_CHECK(vkCreateSampler(_vkDevice, &vkSamplerCreateInfo, nullptr,
                           &_vkAlbedoTextureSampler));

  _deletionQueue.pushFunction([this]() {
    vkDestroySampler(_vkDevice, _vkAlbedoTextureSampler, nullptr);
  });

  VkSamplerCreateInfo const vkDepthSamplerCreateInfo =
      vkinit::samplerCreateInfo(VK_FILTER_LINEAR,
                                VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  VK_CHECK(vkCreateSampler(_vkDevice, &vkDepthSamplerCreateInfo, nullptr,
                           &_vkDepthSampler));

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _vkDepthSampler, nullptr); });

  _lightDataBuffer = createBuffer(
      frameOverlap * getPaddedBufferSize(sizeof(GPULightData)),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  _deletionQueue.pushFunction([this]() {
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
          frameData.shadowMapImageViews[j];
      vkShadowMapDescriptorImageInfos[j].sampler = _vkDepthSampler;
    }

    VkDescriptorBufferInfo vkLightDataBufferInfo = {};
    vkLightDataBufferInfo.buffer = _lightDataBuffer.buffer;
    vkLightDataBufferInfo.offset = 0;
    vkLightDataBufferInfo.range = sizeof(GPULightData);

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .bindImages(0, vkShadowMapDescriptorImageInfos,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(1, vkLightDataBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkDefaultRenderPassDescriptorSet,
               _vkLitMeshRenderPassDescriptorSetLayout);

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .build(frameData.vkObjectDataDescriptorSet,
               _vkObjectDataDescriptorSetLayout);
  }

  VkDescriptorSetLayoutBinding albedoTexBinding = {};
  albedoTexBinding.binding = 0;
  albedoTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  albedoTexBinding.descriptorCount = 1;
  albedoTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo texMatDescriptorSetLayoutCreateInfo = {};
  texMatDescriptorSetLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  texMatDescriptorSetLayoutCreateInfo.bindingCount = 1;
  texMatDescriptorSetLayoutCreateInfo.pBindings = &albedoTexBinding;
  _vkTexturedMaterialDescriptorSetLayout =
      _descriptorLayoutCache.getLayout(texMatDescriptorSetLayoutCreateInfo);
}

void VulkanRHI::initDepthPrepassDescriptors() {
  VkDescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = _cameraBuffer.buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = getPaddedBufferSize(sizeof(GPUCameraData));

  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_depthPrepassGlobalDescriptorSet,
             _vkDepthPassGlobalDescriptorSetLayout);
}

void VulkanRHI::initShadowPassDescriptors() {
  VkDescriptorBufferInfo vkCameraDatabufferInfo = {};
  vkCameraDatabufferInfo.buffer = _shadowPassCameraBuffer.buffer;
  vkCameraDatabufferInfo.offset = 0;
  vkCameraDatabufferInfo.range = sizeof(GPUCameraData);
  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, vkCameraDatabufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_vkShadowPassGlobalDescriptorSet);
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
    depthDescriptorImageInfo.imageView = frameData.vkDepthPrepassImageView;
    depthDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthDescriptorImageInfo.sampler = _vkDepthSampler;

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
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

void VulkanRHI::createDepthImage(AllocatedImage& outImage,
                                 VkImageUsageFlags flags) const {
  VkExtent3D const depthExtent{_windowExtent.width, _windowExtent.height, 1};

  VkImageCreateInfo const depthBufferImageCreateInfo =
      vkinit::imageCreateInfo(flags, depthExtent, _depthFormat);

  VmaAllocationCreateInfo depthImageAllocInfo = {};
  depthImageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  depthImageAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vmaCreateImage(_vmaAllocator, &depthBufferImageCreateInfo,
                 &depthImageAllocInfo, &outImage.vkImage, &outImage.allocation,
                 nullptr);
}

void VulkanRHI::initSSAOSamplesAndNoise() {
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
    samples[i] = {uniformDistribution(randomDevice) * 2.0f - 1.0f,
                  uniformDistribution(randomDevice) * 2.0 - 1.0f,
                  uniformDistribution(randomDevice), 0.0f};
  }

  vmaUnmapMemory(_vmaAllocator, _ssaoSamplesBuffer.allocation);

  _deletionQueue.pushFunction([this]() {
    vmaDestroyBuffer(_vmaAllocator, _ssaoSamplesBuffer.buffer,
                     _ssaoSamplesBuffer.allocation);
  });

  // Using glm::vec2 because the z is assumed to be 0. The reason is because we
  // are using the noise vector to create otrhonormal basis with normal as the z
  // axis. That orthonormal basis will be used as the tangent space for the
  // fragment when calculating ambient occlusion.
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
  uploadTextureRHI.unpackFunc = [&noiseVectors](char* dst) {
    std::memcpy(dst, reinterpret_cast<char*>(noiseVectors.data()),
                noiseVectors.size() *
                    sizeof(decltype(noiseVectors)::value_type));
  };

  _ssaoNoiseTextureID = uploadTexture(uploadTextureRHI);

  _deletionQueue.pushFunction([this]() { unloadTexture(_ssaoNoiseTextureID); });

  VkSamplerCreateInfo samplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_REPEAT);

  vkCreateSampler(_vkDevice, &samplerCreateInfo, nullptr, &_ssaoNoiseSampler);

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _ssaoNoiseSampler, nullptr); });
}
