#include "vk_descriptors.hpp"
#include "vk_mesh.hpp"
#include "vk_types.hpp"
#include <renderdoc.hpp>
#include <vk_check.hpp>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <SDL2/SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

void VulkanEngine::init() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_WINDOW_MOUSE_CAPTURE);
  SDL_WindowFlags windowFlags =
      static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
  Window = SDL_CreateWindow("Obsidian Engine", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WindowExtent.width,
                            WindowExtent.height, windowFlags);

  renderdoc::initRenderdoc();

  initVulkan();

  initSwapchain();

  initCommands();

  initDefaultRenderPass();

  initShadowRenderPass();

  initFramebuffers();

  initShadowPassFramebuffers();

  initSyncStructures();

  loadTextures();

  initDescriptors();

  initShadowPassDescriptors();

  initPipelines();

  loadMeshes();

  initScene();

  IsInitialized = true;
}

void VulkanEngine::initVulkan() {
  vkb::InstanceBuilder builder;

#ifdef USE_VULKAN_VALIDATION_LAYERS
  constexpr bool enable_validation_layers = true;
#else
  constexpr bool enable_validation_layers = false;
#endif

  auto const builderReturn = builder.set_app_name("Obsidian Engine")
                                 .request_validation_layers(true)
                                 .require_api_version(1, 2, 0)
                                 .use_default_debug_messenger()
                                 .build();

  vkb::Instance vkbInstance = builderReturn.value();

  _vkInstance = vkbInstance.instance;
  _vkDebugMessenger = vkbInstance.debug_messenger;

  SDL_Vulkan_CreateSurface(Window, _vkInstance, &_vkSurface);
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

void VulkanEngine::initSwapchain() {
  vkb::SwapchainBuilder swapchainBuilder{_vkPhysicalDevice, _vkDevice,
                                         _vkSurface};

  vkb::Swapchain vkbSwapchain =
      swapchainBuilder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(WindowExtent.width, WindowExtent.height)
          .build()
          .value();
  _vkSwapchain = vkbSwapchain.swapchain;
  _vkSwapchainImages = vkbSwapchain.get_images().value();
  std::vector<VkImageView> swapchainColorImageViews =
      vkbSwapchain.get_image_views().value();

  std::size_t const swapchainSize = swapchainColorImageViews.size();

  _vkFramebufferImageViews.resize(swapchainSize);

  _vkSwapchainImageFormat = vkbSwapchain.image_format;

  _deletionQueue.pushFunction([this, swapchainColorImageViews]() {
    for (VkImageView const& vkSwapchainImgView : swapchainColorImageViews) {
      vkDestroyImageView(_vkDevice, vkSwapchainImgView, nullptr);
    }

    vkDestroySwapchainKHR(_vkDevice, _vkSwapchain, nullptr);
  });

  VkImageUsageFlags depthUsageFlags =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkExtent3D const depthExtent{WindowExtent.width, WindowExtent.height, 1};

  _vkFramebuffers.resize(swapchainSize);

  _depthFormat = VK_FORMAT_D32_SFLOAT;
  VkImageCreateInfo const depthBufferImageCreateInfo =
      vkinit::imageCreateInfo(depthUsageFlags, depthExtent, _depthFormat);

  VmaAllocationCreateInfo depthImageAllocInfo = {};
  depthImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  depthImageAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vmaCreateImage(_vmaAllocator, &depthBufferImageCreateInfo,
                 &depthImageAllocInfo, &_depthImage.vkImage,
                 &_depthImage.allocation, nullptr);

  VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageViewCreateInfo(
      _depthImage.vkImage, _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

  for (int i = 0; i < swapchainSize; ++i) {
    FramebufferImageViews& swapchainImageViews = _vkFramebufferImageViews.at(i);
    std::vector<VkImageView>& imageViews = swapchainImageViews.vkImageViews;

    imageViews.push_back(swapchainColorImageViews[i]);
    VkImageView& depthImageView =
        swapchainImageViews.vkImageViews.emplace_back();

    VK_CHECK(vkCreateImageView(_vkDevice, &imageViewCreateInfo, nullptr,
                               &depthImageView));

    _deletionQueue.pushFunction([this, depthImageView]() {
      vkDestroyImageView(_vkDevice, depthImageView, nullptr);
    });
  }

  _deletionQueue.pushFunction([this]() {
    vmaDestroyImage(_vmaAllocator, _depthImage.vkImage, _depthImage.allocation);
  });
}

void VulkanEngine::initCommands() {
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

void VulkanEngine::initDefaultRenderPass() {
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
                              &_vkRenderPass));

  _deletionQueue.pushFunction(
      [this]() { vkDestroyRenderPass(_vkDevice, _vkRenderPass, nullptr); });
}

void VulkanEngine::initShadowRenderPass() {
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
                              &_vkShadowRenderPass));

  _deletionQueue.pushFunction([this]() {
    vkDestroyRenderPass(_vkDevice, _vkShadowRenderPass, nullptr);
  });
}

void VulkanEngine::initFramebuffers() {
  std::size_t const swapchainImageCount = _vkFramebufferImageViews.size();
  VkFramebufferCreateInfo vkFramebufferCreateInfo = {};
  vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  vkFramebufferCreateInfo.pNext = nullptr;
  vkFramebufferCreateInfo.renderPass = _vkRenderPass;
  vkFramebufferCreateInfo.width = WindowExtent.width;
  vkFramebufferCreateInfo.height = WindowExtent.height;
  vkFramebufferCreateInfo.layers = 1;

  for (int i = 0; i < swapchainImageCount; ++i) {
    FramebufferImageViews& swapchainImageViews = _vkFramebufferImageViews.at(i);
    vkFramebufferCreateInfo.attachmentCount =
        swapchainImageViews.vkImageViews.size();
    vkFramebufferCreateInfo.pAttachments =
        swapchainImageViews.vkImageViews.data();

    VK_CHECK(vkCreateFramebuffer(_vkDevice, &vkFramebufferCreateInfo, nullptr,
                                 &_vkFramebuffers[i]));

    _deletionQueue.pushFunction([this, i]() {
      vkDestroyFramebuffer(_vkDevice, _vkFramebuffers[i], nullptr);
    });
  }
}

void VulkanEngine::initShadowPassFramebuffers() {
  for (std::size_t i = 0; i < _frameDataArray.size(); ++i) {
    AllocatedImage& imageShadowPassAttachment =
        _frameDataArray[i].shadowMapImage;

    VkExtent3D vkShadowPassAttachmentExtent = {shadowPassAttachmentWidth,
                                               shadowPassAttachmentHeight, 1};
    VkImageCreateInfo const vkImageShadowPassAttachmentCreateInfo =
        vkinit::imageCreateInfo(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_SAMPLED_BIT,
                                vkShadowPassAttachmentExtent, _depthFormat);

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.flags = 0;
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
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
    VK_CHECK(vkCreateImageView(_vkDevice, &shadowMapImageViewCreateInfo,
                               nullptr,
                               &_frameDataArray[i].shadowMapImageView));

    _deletionQueue.pushFunction(
        [this, view = _frameDataArray[i].shadowMapImageView]() {
          vkDestroyImageView(_vkDevice, view, nullptr);
        });

    VkImageView vkShadowPassAttachmentImgView;

    VkImageViewCreateInfo vkImageViewCreateInfo = {};
    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext = nullptr;

    vkImageViewCreateInfo.image = imageShadowPassAttachment.vkImage;
    vkImageViewCreateInfo =
        vkinit::imageViewCreateInfo(imageShadowPassAttachment.vkImage,
                                    _depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_vkDevice, &vkImageViewCreateInfo, nullptr,
                               &vkShadowPassAttachmentImgView));

    _deletionQueue.pushFunction([this, vkShadowPassAttachmentImgView]() {
      vkDestroyImageView(_vkDevice, vkShadowPassAttachmentImgView, nullptr);
    });

    VkFramebufferCreateInfo vkFramebufferCreateInfo = {};

    vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    vkFramebufferCreateInfo.pNext = nullptr;

    vkFramebufferCreateInfo.renderPass = _vkShadowRenderPass;
    vkFramebufferCreateInfo.attachmentCount = 1;
    vkFramebufferCreateInfo.pAttachments = &vkShadowPassAttachmentImgView;
    vkFramebufferCreateInfo.width = shadowPassAttachmentWidth;
    vkFramebufferCreateInfo.height = shadowPassAttachmentHeight;
    vkFramebufferCreateInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(_vkDevice, &vkFramebufferCreateInfo, nullptr,
                                 &_frameDataArray[i].shadowFrameBuffer));

    _deletionQueue.pushFunction(
        [this, fb = _frameDataArray[i].shadowFrameBuffer]() {
          vkDestroyFramebuffer(_vkDevice, fb, nullptr);
        });
  }
}

void VulkanEngine::initSyncStructures() {
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

void VulkanEngine::initPipelines() {
  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkVertexInputInfo = vkinit::vertexInputStateCreateInfo();
  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder._vkDepthStencilStateCreateInfo =
      vkinit::depthStencilStateCreateInfo(VK_TRUE);

  pipelineBuilder._vkViewport.x = 0.0f;
  pipelineBuilder._vkViewport.y = 0.0f;
  pipelineBuilder._vkViewport.height = WindowExtent.height;
  pipelineBuilder._vkViewport.width = WindowExtent.width;
  pipelineBuilder._vkViewport.minDepth = 0.0f;
  pipelineBuilder._vkViewport.maxDepth = 1.0f;

  pipelineBuilder._vkScissor.offset = {0, 0};
  pipelineBuilder._vkScissor.extent = WindowExtent;

  pipelineBuilder._vkRasterizationCreateInfo =
      vkinit::rasterizationCreateInfo(VK_POLYGON_MODE_FILL);
  pipelineBuilder._vkColorBlendAttachmentState =
      vkinit::colorBlendAttachmentState();
  pipelineBuilder._vkMultisampleStateCreateInfo =
      vkinit::multisampleStateCreateInfo();

  VertexInputDescription vertexDescription =
      Vertex::getVertexInputDescription();

  pipelineBuilder._vkVertexInputInfo.vertexAttributeDescriptionCount =
      vertexDescription.attributes.size();
  pipelineBuilder._vkVertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.attributes.data();

  pipelineBuilder._vkVertexInputInfo.vertexBindingDescriptionCount =
      vertexDescription.bindings.size();
  pipelineBuilder._vkVertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.bindings.data();

  VkShaderModule meshVertShader;

  if (!loadShaderModule("shaders/mesh.vert.spv", &meshVertShader)) {
    std::cout << "Error when building the mesh vertex shader module"
              << std::endl;
  } else {
    std::cout << "Mesh vertex shader successfully loaded" << std::endl;
  }

  VkShaderModule meshFragShader;

  if (!loadShaderModule("shaders/mesh.frag.spv", &meshFragShader)) {
    std::cout << "Error when building the mesh fragment shader module"
              << std::endl;
  } else {
    std::cout << "Mesh fragment shader successfully loaded" << std::endl;
  }

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            meshVertShader));

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            meshFragShader));

  VkPipelineLayoutCreateInfo meshPipelineLayoutInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> const meshDescriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkLitMeshrenderPassDescriptorSetLayout,
      _vkTexturedMaterialDescriptorSetLayout, _vkObjectDataDescriptorSetLayout};

  meshPipelineLayoutInfo.setLayoutCount = meshDescriptorSetLayouts.size();
  meshPipelineLayoutInfo.pSetLayouts = meshDescriptorSetLayouts.data();

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &meshPipelineLayoutInfo, nullptr,
                                  &_vkMeshPipelineLayout));

  pipelineBuilder._vkPipelineLayout = _vkMeshPipelineLayout;
  _vkMeshPipeline = pipelineBuilder.buildPipeline(_vkDevice, _vkRenderPass);

  createMaterial(_vkMeshPipeline, _vkMeshPipelineLayout, "defaultmesh");

  vkDestroyShaderModule(_vkDevice, meshVertShader, nullptr);
  vkDestroyShaderModule(_vkDevice, meshFragShader, nullptr);

  _deletionQueue.pushFunction(
      [this] { vkDestroyPipeline(_vkDevice, _vkMeshPipeline, nullptr); });

  pipelineBuilder._vkShaderStageCreateInfo.clear();

  _deletionQueue.pushFunction([this] {
    vkDestroyPipelineLayout(_vkDevice, _vkMeshPipelineLayout, nullptr);
  });

  // Lit mesh pipeline

  VkShaderModule litMeshVertShader;

  if (!loadShaderModule("shaders/mesh-light.vert.dbg.spv",
                        &litMeshVertShader)) {
    std::cout << "Error when building the lit mesh vertex shader module"
              << std::endl;
  } else {
    std::cout << "Lit mesh vertex shader successfully loaded" << std::endl;
  }

  VkShaderModule litMeshFragShader;

  if (!loadShaderModule("shaders/mesh-light.frag.dbg.spv",
                        &litMeshFragShader)) {
    std::cout << "Error when building the lit mesh fragment shader module"
              << std::endl;
  } else {
    std::cout << "Lit mesh fragment shader successfully loaded" << std::endl;
  }

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            litMeshVertShader));

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            litMeshFragShader));

  VkPipelineLayoutCreateInfo litMeshPipelineLayoutCreateInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 4> vkLitMeshPipelineLayouts = {
      _vkGlobalDescriptorSetLayout,
      _vkLitMeshrenderPassDescriptorSetLayout,
      _vkTexturedMaterialDescriptorSetLayout,
      _vkObjectDataDescriptorSetLayout,
  };

  litMeshPipelineLayoutCreateInfo.pSetLayouts = vkLitMeshPipelineLayouts.data();
  litMeshPipelineLayoutCreateInfo.setLayoutCount =
      vkLitMeshPipelineLayouts.size();

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &litMeshPipelineLayoutCreateInfo,
                                  nullptr, &_vkLitMeshPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkLitMeshPipelineLayout, nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkLitMeshPipelineLayout;

  _vkLitMeshPipeline = pipelineBuilder.buildPipeline(_vkDevice, _vkRenderPass);

  createMaterial(_vkLitMeshPipeline, _vkMeshPipelineLayout, "lit-mesh",
                 "lost-empire");

  vkDestroyShaderModule(_vkDevice, litMeshVertShader, nullptr);
  vkDestroyShaderModule(_vkDevice, litMeshFragShader, nullptr);

  _deletionQueue.pushFunction(
      [this] { vkDestroyPipeline(_vkDevice, _vkLitMeshPipeline, nullptr); });

  pipelineBuilder._vkShaderStageCreateInfo.clear();

  // Shadow pass pipeline

  pipelineBuilder._vkRasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

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

  pipelineBuilder._vkVertexInputInfo.vertexBindingDescriptionCount =
      shadowPassVertexInputDescription.bindings.size();
  pipelineBuilder._vkVertexInputInfo.pVertexBindingDescriptions =
      shadowPassVertexInputDescription.bindings.data();

  pipelineBuilder._vkVertexInputInfo.vertexAttributeDescriptionCount =
      shadowPassVertexInputDescription.attributes.size();
  pipelineBuilder._vkVertexInputInfo.pVertexAttributeDescriptions =
      shadowPassVertexInputDescription.attributes.data();

  VkShaderModule shadowPassVertShader;

  if (!loadShaderModule("shaders/shadow-pass.vert.spv",
                        &shadowPassVertShader)) {
    std::cout << "Error when building the shadow pass vertex shader module"
              << std::endl;
  } else {
    std::cout << "Shadow pass vertex shader successfully loaded" << std::endl;
  }

  VkShaderModule shadowPassFragShader;

  if (!loadShaderModule("shaders/empty.frag.spv", &shadowPassFragShader)) {
    std::cout << "Error when building the empty fragment shader module"
              << std::endl;
  } else {
    std::cout << "Empty fragment shader successfully loaded" << std::endl;
  }

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            shadowPassVertShader));

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            shadowPassFragShader));

  VkPipelineLayoutCreateInfo vkShadowPassPipelineLayoutCreateInfo = {};
  vkShadowPassPipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  vkShadowPassPipelineLayoutCreateInfo.pNext = nullptr;

  std::array<VkDescriptorSetLayout, 4> shadowPassDescriptorSetLayouts = {
      _vkShadowPassGlobalDescriptorSetLayout, _vkEmptyDescriptorSetLayout,
      _vkEmptyDescriptorSetLayout, _vkObjectDataDescriptorSetLayout};

  vkShadowPassPipelineLayoutCreateInfo.setLayoutCount =
      shadowPassDescriptorSetLayouts.size();
  vkShadowPassPipelineLayoutCreateInfo.pSetLayouts =
      shadowPassDescriptorSetLayouts.data();

  VK_CHECK(vkCreatePipelineLayout(_vkDevice,
                                  &vkShadowPassPipelineLayoutCreateInfo,
                                  nullptr, &_vkShadowPassPipelineLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipelineLayout(_vkDevice, _vkShadowPassPipelineLayout, nullptr);
  });

  pipelineBuilder._vkPipelineLayout = _vkShadowPassPipelineLayout;

  _vkShadowPassPipeline =
      pipelineBuilder.buildPipeline(_vkDevice, _vkShadowRenderPass);

  _deletionQueue.pushFunction([this]() {
    vkDestroyPipeline(_vkDevice, _vkShadowPassPipeline, nullptr);
  });

  vkDestroyShaderModule(_vkDevice, shadowPassVertShader, nullptr);
  vkDestroyShaderModule(_vkDevice, shadowPassFragShader, nullptr);
}

void VulkanEngine::initScene() {
  RenderObject& monkey = _renderObjects.emplace_back();
  monkey.mesh = getMesh("monkey");
  monkey.material = getMaterial("defaultmesh");
  monkey.transformMatrix = glm::mat4{1.0f};

  RenderObject& lostEmpire = _renderObjects.emplace_back();
  lostEmpire.mesh = getMesh("lost-empire");
  lostEmpire.material = getMaterial("lit-mesh");
  lostEmpire.transformMatrix = glm::mat4{1.f};

  for (int x = -20; x <= 20; ++x) {
    for (int y = -20; y <= 20; ++y) {
      RenderObject triangle;

      triangle.mesh = getMesh("triangle");
      triangle.material = getMaterial("defaultmesh");

      glm::mat4 const translation =
          glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y));
      glm::mat4 const scale =
          glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));
      triangle.transformMatrix = translation * scale;

      _renderObjects.push_back(triangle);
    }
  }
}

void VulkanEngine::initDescriptors() {
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
      createBuffer(frameOverlap * getPaddedBufferSize(sizeof(GPUCameraData)),
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
                  VK_SHADER_STAGE_VERTEX_BIT)
      .bindBuffer(1, sceneDescriptorBufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(_vkGlobalDescriptorSet, _vkGlobalDescriptorSetLayout);

  VkSamplerCreateInfo vkSamplerCreateInfo = vkinit::samplerCreateInfo(
      VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  VK_CHECK(
      vkCreateSampler(_vkDevice, &vkSamplerCreateInfo, nullptr, &_vkSampler));

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _vkSampler, nullptr); });

  for (int i = 0; i < frameOverlap; ++i) {
    FrameData& frameData = _frameDataArray[i];

    VkSamplerCreateInfo const vkShadowMapSamplerCreateInfo =
        vkinit::samplerCreateInfo(VK_FILTER_NEAREST,
                                  VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VK_CHECK(vkCreateSampler(_vkDevice, &vkShadowMapSamplerCreateInfo, nullptr,
                             &frameData.shadowMapSampler));

    _deletionQueue.pushFunction([this, frameData]() {
      vkDestroySampler(_vkDevice, frameData.shadowMapSampler, nullptr);
    });

    VkDescriptorImageInfo vkShadowMapDescriptorImageInfo = {};
    vkShadowMapDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkShadowMapDescriptorImageInfo.imageView = frameData.shadowMapImageView;
    vkShadowMapDescriptorImageInfo.sampler = frameData.shadowMapSampler;

    VkDescriptorBufferInfo vkShadowCameraDataBufferInfo = {};
    vkShadowCameraDataBufferInfo.buffer = _shadowPassCameraBuffer.buffer;
    vkShadowCameraDataBufferInfo.offset = 0;
    vkShadowCameraDataBufferInfo.range =
        getPaddedBufferSize(sizeof(GPUCameraData));

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .bindImage(0, vkShadowMapDescriptorImageInfo,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(1, vkShadowCameraDataBufferInfo,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(frameData.vkDefaultRenderPassDescriptorSet,
               _vkLitMeshrenderPassDescriptorSetLayout);

    frameData.vkObjectDataBuffer =
        createBuffer(maxNumberOfObjects * sizeof(GPUObjectData),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    _deletionQueue.pushFunction([this, frameData]() {
      AllocatedBuffer const& buffer = frameData.vkObjectDataBuffer;
      vmaDestroyBuffer(_vmaAllocator, buffer.buffer, buffer.allocation);
    });

    VkDescriptorBufferInfo objectDataDescriptorBufferInfo = {};
    objectDataDescriptorBufferInfo.buffer = frameData.vkObjectDataBuffer.buffer;
    objectDataDescriptorBufferInfo.offset = 0;
    objectDataDescriptorBufferInfo.range = VK_WHOLE_SIZE;

    DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                             _descriptorLayoutCache)
        .bindBuffer(0, objectDataDescriptorBufferInfo,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
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

void VulkanEngine::initShadowPassDescriptors() {
  VkDescriptorBufferInfo vkCameraDatabufferInfo = {};
  vkCameraDatabufferInfo.buffer = _shadowPassCameraBuffer.buffer;
  vkCameraDatabufferInfo.offset = 0;
  vkCameraDatabufferInfo.range = sizeof(GPUCameraData);
  DescriptorBuilder::begin(_vkDevice, _descriptorAllocator,
                           _descriptorLayoutCache)
      .bindBuffer(0, vkCameraDatabufferInfo,
                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .build(_vkShadowPassGlobalDescriptorSet,
             _vkShadowPassGlobalDescriptorSetLayout);
}
