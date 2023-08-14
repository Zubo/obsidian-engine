#include <renderdoc.hpp>
#include <vk_check.hpp>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <SDL2/SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

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

  initFramebuffers();

  initSyncStructures();

  loadTextures();

  initDescriptors();

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
    std::cout << "Error when building the triangle vertex shader module"
              << std::endl;
  } else {
    std::cout << "Mesh triangle vertex shader successfully loaded" << std::endl;
  }

  VkShaderModule meshFragShader;

  if (!loadShaderModule("shaders/mesh.frag.spv", &meshFragShader)) {
    std::cout << "Error when building the triangle fragment shader module"
              << std::endl;
  } else {
    std::cout << "Mesh triangle fragment shader successfully loaded"
              << std::endl;
  }

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            meshVertShader));

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            meshFragShader));

  VkPipelineLayoutCreateInfo meshPipelineLayoutInfo =
      vkinit::pipelineLayoutCreateInfo();

  std::array<VkDescriptorSetLayout, 2> const descriptorSetLayouts = {
      _vkGlobalDescriptorSetLayout, _vkObjectDataDescriptorSetLayout};

  meshPipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
  meshPipelineLayoutInfo.pSetLayouts = &_vkGlobalDescriptorSetLayout;

  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &meshPipelineLayoutInfo, nullptr,
                                  &_vkMeshPipelineLayout));

  pipelineBuilder._vkPipelineLayout = _vkMeshPipelineLayout;
  _vkMeshPipeline = pipelineBuilder.buildPipeline(_vkDevice, _vkRenderPass);

  createMaterial(_vkMeshPipeline, _vkMeshPipelineLayout, "defaultmesh");

  vkDestroyShaderModule(_vkDevice, meshVertShader, nullptr);
  vkDestroyShaderModule(_vkDevice, meshFragShader, nullptr);

  _deletionQueue.pushFunction([this] {
    vkDestroyPipeline(_vkDevice, _vkMeshPipeline, nullptr);
    vkDestroyPipelineLayout(_vkDevice, _vkMeshPipelineLayout, nullptr);
  });
}

void VulkanEngine::initScene() {
  RenderObject& monkey = _renderObjects.emplace_back();
  monkey.mesh = getMesh("monkey");
  monkey.material = getMaterial("defaultmesh");
  monkey.transformMatrix = glm::mat4{1.0f};

  RenderObject& lostEmpire = _renderObjects.emplace_back();
  lostEmpire.mesh = getMesh("lostEmpire");
  lostEmpire.material = getMaterial("defaultmesh");
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
  constexpr std::uint32_t descriptorPoolSize{10};

  std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorPoolSize},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descriptorPoolSize},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorPoolSize},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorPoolSize}};

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.pNext = nullptr;
  descriptorPoolCreateInfo.maxSets =
      descriptorPoolSizes.size() * descriptorPoolSize;
  descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

  VK_CHECK(vkCreateDescriptorPool(_vkDevice, &descriptorPoolCreateInfo, nullptr,
                                  &_vkDescriptorPool));

  _deletionQueue.pushFunction([this]() {
    vkDestroyDescriptorPool(_vkDevice, _vkDescriptorPool, nullptr);
  });

  VkDescriptorSetLayoutBinding bindings[2];
  VkDescriptorSetLayoutBinding& camBufferBinding = bindings[0];
  camBufferBinding = vkinit::descriptorSetLayoutBinding(
      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);

  VkDescriptorSetLayoutBinding& sceneDataBufferBinding = bindings[1];
  sceneDataBufferBinding = vkinit::descriptorSetLayoutBinding(
      1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDescriptorSetLayoutCreateInfo layoutCreateInfo =
      vkinit::descriptorSetLayoutCreateInfo(bindings, sizeof(bindings) /
                                                          sizeof(bindings[0]));

  VK_CHECK(vkCreateDescriptorSetLayout(_vkDevice, &layoutCreateInfo, nullptr,
                                       &_vkGlobalDescriptorSetLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyDescriptorSetLayout(_vkDevice, _vkGlobalDescriptorSetLayout,
                                 nullptr);
  });

  std::array<VkDescriptorSetLayoutBinding, 2> perObjectBindings;
  VkDescriptorSetLayoutBinding& objectDataBinding = perObjectBindings[0];
  objectDataBinding = vkinit::descriptorSetLayoutBinding(
      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

  VkDescriptorSetLayoutBinding& textureBinding = perObjectBindings[1];
  textureBinding = vkinit::descriptorSetLayoutBinding(
      1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDescriptorSetLayoutCreateInfo objectLayoutCreateInfo =
      vkinit::descriptorSetLayoutCreateInfo(perObjectBindings.data(),
                                            perObjectBindings.size());

  VK_CHECK(vkCreateDescriptorSetLayout(_vkDevice, &objectLayoutCreateInfo,
                                       nullptr,
                                       &_vkObjectDataDescriptorSetLayout));

  _deletionQueue.pushFunction([this]() {
    vkDestroyDescriptorSetLayout(_vkDevice, _vkObjectDataDescriptorSetLayout,
                                 nullptr);
  });

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

  VkDescriptorSetAllocateInfo globalDescriptorSetAllocateInfo =
      vkinit::descriptorSetAllocateInfo(_vkDescriptorPool,
                                        &_vkGlobalDescriptorSetLayout, 1);

  VK_CHECK(vkAllocateDescriptorSets(_vkDevice, &globalDescriptorSetAllocateInfo,
                                    &_globalDescriptorSet));

  VkDescriptorBufferInfo cameraDescriptorBufferInfo;
  cameraDescriptorBufferInfo.buffer = _cameraBuffer.buffer;
  cameraDescriptorBufferInfo.offset = 0;
  cameraDescriptorBufferInfo.range = sizeof(GPUCameraData);

  VkDescriptorBufferInfo sceneDescriptorBufferInfo;
  sceneDescriptorBufferInfo.buffer = _sceneDataBuffer.buffer;
  sceneDescriptorBufferInfo.offset = 0;
  sceneDescriptorBufferInfo.range = sizeof(GPUSceneData);

  std::vector<VkWriteDescriptorSet> descriptorSetWrites = {
      vkinit::writeDescriptorSet(_globalDescriptorSet,
                                 &cameraDescriptorBufferInfo, 1, nullptr, 0,
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0),
      vkinit::writeDescriptorSet(_globalDescriptorSet,
                                 &sceneDescriptorBufferInfo, 1, nullptr, 0,
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1)};

  vkUpdateDescriptorSets(_vkDevice, descriptorSetWrites.size(),
                         descriptorSetWrites.data(), 0, nullptr);

  VkSamplerCreateInfo vkSamplerCreateInfo = {};
  vkSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  vkSamplerCreateInfo.pNext = nullptr;

  vkSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  vkSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
  vkSamplerCreateInfo.maxAnisotropy = 8.f;

  VK_CHECK(
      vkCreateSampler(_vkDevice, &vkSamplerCreateInfo, nullptr, &_vkSampler));

  _deletionQueue.pushFunction(
      [this]() { vkDestroySampler(_vkDevice, _vkSampler, nullptr); });

  for (int i = 0; i < frameOverlap; ++i) {
    _frameDataArray[i].objectDataBuffer =
        createBuffer(maxNumberOfObjects * sizeof(GPUObjectData),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    _deletionQueue.pushFunction([this, i]() {
      AllocatedBuffer& buffer = _frameDataArray[i].objectDataBuffer;
      vmaDestroyBuffer(_vmaAllocator, buffer.buffer, buffer.allocation);
    });

    VkDescriptorSetAllocateInfo objectDataDescriptorSetAllocateInfo =
        vkinit::descriptorSetAllocateInfo(_vkDescriptorPool,
                                          &_vkObjectDataDescriptorSetLayout, 1);

    VK_CHECK(vkAllocateDescriptorSets(
        _vkDevice, &objectDataDescriptorSetAllocateInfo,
        &_frameDataArray[i].objectDataDescriptorSet));

    std::array<VkWriteDescriptorSet, 2> writeDescriptorSets;

    VkDescriptorBufferInfo objectDataDescriptorBufferInfo = {};
    objectDataDescriptorBufferInfo.buffer =
        _frameDataArray[i].objectDataBuffer.buffer;
    objectDataDescriptorBufferInfo.offset = 0;
    objectDataDescriptorBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet& writeObjectDataDescriptorSet = writeDescriptorSets[0];
    writeObjectDataDescriptorSet =
        vkinit::writeDescriptorSet(_frameDataArray[i].objectDataDescriptorSet,
                                   &objectDataDescriptorBufferInfo, 1, nullptr,
                                   0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

    VkDescriptorImageInfo textureDescriptorImageInfo = {};
    textureDescriptorImageInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textureDescriptorImageInfo.sampler = _vkSampler;
    textureDescriptorImageInfo.imageView =
        _loadedTextures["lostEmpire"].imageView;

    VkWriteDescriptorSet& writeImageDescriptorSet = writeDescriptorSets[1];
    writeImageDescriptorSet = vkinit::writeDescriptorSet(
        _frameDataArray[i].objectDataDescriptorSet, nullptr, 0,
        &textureDescriptorImageInfo, 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    vkUpdateDescriptorSets(_vkDevice, writeDescriptorSets.size(),
                           writeDescriptorSets.data(), 0, nullptr);
  }
}
