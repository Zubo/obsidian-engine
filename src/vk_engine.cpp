#include "vk_types.hpp"
#include <renderdoc.hpp>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <stb/stb_image.h>
#include <tracy/Tracy.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_structs.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      std::cout << "Detected Vulkan error: " << err << std::endl;              \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

void VulkanEngine::init() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_WINDOW_MOUSE_CAPTURE);
  SDL_WindowFlags windowFlags =
      static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
  Window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WindowExtent.width,
                            WindowExtent.height, windowFlags);

  renderdoc::initRenderdoc();

  initVulkan();

  initSwapchain();

  initCommands();

  initDefaultRenderPass();

  initFramebuffers();

  initSyncStructures();

  initDescriptors();

  initPipelines();

  loadTextures();

  loadMeshes();

  initScene();

  IsInitialized = true;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool shouldQuit = false;

  while (!shouldQuit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_KEYDOWN)
        _selectedShader = (_selectedShader + 1) % 2;
      else if (e.type == SDL_QUIT)
        shouldQuit = true;
    }

    draw();

    FrameMark;
  }
}

void VulkanEngine::cleanup() {
  if (IsInitialized) {
    renderdoc::deinitRenderdoc();
    vkDeviceWaitIdle(_vkDevice);

    _deletionQueue.flush();

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);

    vkDestroyInstance(_vkInstance, nullptr);
    SDL_DestroyWindow(Window);

    IsInitialized = false;
  }
}

void VulkanEngine::initVulkan() {
  vkb::InstanceBuilder builder;

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

bool VulkanEngine::loadShaderModule(char const* filePath,
                                    VkShaderModule* outShaderModule) {
  std::ifstream file{filePath, std::ios::ate | std::ios::binary};

  if (!file.is_open()) {
    return false;
  }

  std::size_t const fileSize = static_cast<std::size_t>(file.tellg());
  std::vector<std::uint32_t> buffer((fileSize + sizeof(std::uint32_t) - 1) /
                                    sizeof(std::uint32_t));

  file.seekg(0);

  file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

  file.close();

  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  shaderModuleCreateInfo.codeSize = fileSize;
  shaderModuleCreateInfo.pCode = buffer.data();

  if (vkCreateShaderModule(_vkDevice, &shaderModuleCreateInfo, nullptr,
                           outShaderModule)) {
    return false;
  }

  return true;
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

  VkDescriptorSetLayout const descriptorSetLayouts[] = {
      _vkGlobalDescriptorSetLayout, _vkObjectDataDescriptorSetLayout};

  meshPipelineLayoutInfo.setLayoutCount =
      sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
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

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass) {
  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
  viewportStateCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateCreateInfo.pNext = nullptr;
  viewportStateCreateInfo.viewportCount = 1;
  viewportStateCreateInfo.pViewports = &_vkViewport;
  viewportStateCreateInfo.scissorCount = 1;
  viewportStateCreateInfo.pScissors = &_vkScissor;

  VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
  colorBlendingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendingCreateInfo.pNext = nullptr;
  colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
  colorBlendingCreateInfo.attachmentCount = 1;
  colorBlendingCreateInfo.pAttachments = &_vkColorBlendAttachmentState;

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
  graphicsPipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCreateInfo.pNext = nullptr;

  graphicsPipelineCreateInfo.stageCount = _vkShaderStageCreateInfo.size();
  graphicsPipelineCreateInfo.pStages = _vkShaderStageCreateInfo.data();
  graphicsPipelineCreateInfo.pVertexInputState = &_vkVertexInputInfo;
  graphicsPipelineCreateInfo.pInputAssemblyState = &_vkInputAssemblyCreateInfo;
  graphicsPipelineCreateInfo.pTessellationState = nullptr;
  graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &_vkRasterizationCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &_vkMultisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pDepthStencilState =
      &_vkDepthStencilStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
  graphicsPipelineCreateInfo.pDynamicState = nullptr;
  graphicsPipelineCreateInfo.layout = _vkPipelineLayout;
  graphicsPipelineCreateInfo.renderPass = pass;
  graphicsPipelineCreateInfo.subpass = 0;
  graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline newPipeline;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                &graphicsPipelineCreateInfo, nullptr,
                                &newPipeline) != VK_SUCCESS) {
    std::cout << "Failed to create pipeline" << std::endl;
    return VK_NULL_HANDLE;
  }

  return newPipeline;
}

void VulkanEngine::initScene() {
  RenderObject monkey;
  monkey.mesh = getMesh("monkey");
  monkey.material = getMaterial("defaultmesh");
  monkey.transformMatrix = glm::mat4{1.0f};

  _renderObjects.push_back(monkey);

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

void VulkanEngine::loadTextures() {
  Texture& lostEmpireTexture = _loadedTextures["empire"];
  bool const lostEmpImageLoaded =
      loadImage("assets/lost_empire-RGBA.png", lostEmpireTexture.image);
  assert(lostEmpImageLoaded);

  VkImageViewCreateInfo lostEmpImgViewCreateInfo = vkinit::imageViewCreateInfo(
      lostEmpireTexture.image.vkImage, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT);

  vkCreateImageView(_vkDevice, &lostEmpImgViewCreateInfo, nullptr,
                    &lostEmpireTexture.imageView);

  _deletionQueue.pushFunction([this, lostEmpireTexture]() {
    vkDestroyImageView(_vkDevice, lostEmpireTexture.imageView, nullptr);
  });
}

void VulkanEngine::loadMeshes() {
  _triangleMesh.vertices.resize(3);
  _triangleMesh.vertices[0].position = {1.0f, 1.0f, 0.0f};
  _triangleMesh.vertices[1].position = {-1.0f, 1.0f, 0.0f};
  _triangleMesh.vertices[2].position = {0.0f, -1.0f, 0.0f};

  _triangleMesh.vertices[0].color = {0.0f, 1.0f, 0.0f};
  _triangleMesh.vertices[1].color = {0.0f, 1.0f, 0.0f};
  _triangleMesh.vertices[2].color = {0.7f, 0.5f, 0.1f};

  uploadMesh(_triangleMesh);

  _monkeyMesh.loadFromObj("assets/monkey_smooth.obj");
  uploadMesh(_monkeyMesh);

  _meshes["monkey"] = _monkeyMesh;
  _meshes["triangle"] = _triangleMesh;
}

void VulkanEngine::uploadMesh(Mesh& mesh) {
  size_t const bufferSize =
      mesh.vertices.size() * sizeof(decltype(mesh.vertices)::value_type);

  AllocatedBuffer stagingBuffer =
      createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  std::memcpy(mappedMemory, mesh.vertices.data(), bufferSize);

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  mesh.vertexBuffer = createBuffer(bufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

  _deletionQueue.pushFunction([this, mesh] {
    vmaDestroyBuffer(_vmaAllocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);
  });

  immediateSubmit(
      [this, &stagingBuffer, &mesh, bufferSize](VkCommandBuffer cmd) {
        VkBufferCopy vkBufferCopy = {};
        vkBufferCopy.srcOffset = 0;
        vkBufferCopy.dstOffset = 0;
        vkBufferCopy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1,
                        &vkBufferCopy);
      });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);
}

FrameData& VulkanEngine::getCurrentFrameData() {
  std::size_t const currentFrameDataInd = _frameNumber % frameOverlap;
  return _frameDataArray[currentFrameDataInd];
}

void VulkanEngine::draw() {
  FrameData& currentFrameData = getCurrentFrameData();

  constexpr std::uint64_t timeoutNanoseconds = 1000000000;
  {
    ZoneScopedN("Wait For Fences");
    VK_CHECK(vkWaitForFences(_vkDevice, 1, &currentFrameData.vkRenderFence,
                             true, timeoutNanoseconds));
  }
  VK_CHECK(vkResetFences(_vkDevice, 1, &currentFrameData.vkRenderFence));

  uint32_t swapchainImageIndex;
  {
    ZoneScopedN("Acquire Next Image");
    VK_CHECK(vkAcquireNextImageKHR(_vkDevice, _vkSwapchain, timeoutNanoseconds,
                                   currentFrameData.vkPresentSemaphore,
                                   VK_NULL_HANDLE, &swapchainImageIndex));
  }

  VkCommandBuffer cmd = currentFrameData.vkCommandBuffer;

  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {};
  vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkCommandBufferBeginInfo.pNext = nullptr;
  vkCommandBufferBeginInfo.pInheritanceInfo = nullptr;
  vkCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &vkCommandBufferBeginInfo));

  VkClearValue clearValues[2];
  float flash = 1.0f; // std::abs(std::sin(_frameNumber / 10.0f));
  clearValues[0].color = {{0.0f, 0.0f, flash, 1.0f}};
  clearValues[1].depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _vkRenderPass;
  vkRenderPassBeginInfo.framebuffer = _vkFramebuffers[swapchainImageIndex];
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = WindowExtent;
  vkRenderPassBeginInfo.clearValueCount = 2;
  vkRenderPassBeginInfo.pClearValues = clearValues;

  vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  drawObjects(cmd, _renderObjects.data(), _renderObjects.size());

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo vkSubmitInfo = vkinit::commandBufferSubmitInfo(&cmd);

  VkPipelineStageFlags const vkPipelineStageFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  vkSubmitInfo.pWaitDstStageMask = &vkPipelineStageFlags;
  vkSubmitInfo.waitSemaphoreCount = 1;
  vkSubmitInfo.pWaitSemaphores = &currentFrameData.vkPresentSemaphore;

  vkSubmitInfo.signalSemaphoreCount = 1;
  vkSubmitInfo.pSignalSemaphores = &currentFrameData.vkRenderSemaphore;

  VK_CHECK(vkQueueSubmit(_vkGraphicsQueue, 1, &vkSubmitInfo,
                         currentFrameData.vkRenderFence));

  VkPresentInfoKHR vkPresentInfo = {};
  vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  vkPresentInfo.pNext = nullptr;

  vkPresentInfo.pSwapchains = &_vkSwapchain;
  vkPresentInfo.swapchainCount = 1;

  vkPresentInfo.waitSemaphoreCount = 1;
  vkPresentInfo.pWaitSemaphores = &currentFrameData.vkRenderSemaphore;

  vkPresentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_vkGraphicsQueue, &vkPresentInfo));

  ++_frameNumber;
}

void VulkanEngine::drawObjects(VkCommandBuffer cmd, RenderObject* first,
                               int count) {
  ZoneScoped;
  glm::vec3 const cameraPos{0.f, -6.f, -10.f};
  glm::mat4 view = glm::translate(glm::mat4{1.f}, cameraPos);
  glm::mat4 projection =
      glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.f);
  projection[1][1] *= -1;
  glm::mat4 const viewProjection = projection * view;

  GPUCameraData gpuCameraData;
  gpuCameraData.view = view;
  gpuCameraData.proj = projection;
  gpuCameraData.viewProj = projection * view;

  FrameData& currentFrameData = getCurrentFrameData();

  std::size_t const frameInd = _frameNumber % frameOverlap;

  void* data = nullptr;

  VK_CHECK(vmaMapMemory(_vmaAllocator, _cameraBuffer.allocation, &data));

  char* const dstGPUCameraData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUCameraData));

  std::memcpy(dstGPUCameraData, &gpuCameraData, sizeof(gpuCameraData));

  vmaUnmapMemory(_vmaAllocator, _cameraBuffer.allocation);

  GPUSceneData gpuSceneData;
  gpuSceneData.ambientColor = {
      1.0, 1 - std::abs(std::sin(_frameNumber / 20.0f)), 1.0, 1.0};

  VK_CHECK(vmaMapMemory(_vmaAllocator, _sceneDataBuffer.allocation,
                        reinterpret_cast<void**>(&data)));

  char* const dstGPUSceneData =
      reinterpret_cast<char*>(data) +
      frameInd * getPaddedBufferSize(sizeof(GPUSceneData));

  std::memcpy(dstGPUSceneData, &gpuSceneData, sizeof(GPUSceneData));

  vmaUnmapMemory(_vmaAllocator, _sceneDataBuffer.allocation);

  vmaMapMemory(_vmaAllocator, currentFrameData.objectDataBuffer.allocation,
               &data);

  GPUObjectData* objectData = reinterpret_cast<GPUObjectData*>(data);

  for (int i = 0; i < _renderObjects.size(); ++i) {
    objectData[i].modelMat = _renderObjects[i].transformMatrix;
  }

  vmaUnmapMemory(_vmaAllocator, currentFrameData.objectDataBuffer.allocation);

  Material const* lastMaterial;
  for (int i = 0; i < count; ++i) {
    ZoneScopedN("Draw Object");
    RenderObject const& obj = first[i];

    assert(obj.material && "Error: Missing material.");
    Material const& material = *obj.material;

    assert(obj.mesh && "Error: Missing mesh");
    Mesh const& mesh = *obj.mesh;

    constexpr VkPipelineBindPoint pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    if (&material != lastMaterial) {
      std::uint32_t const offsets[] = {
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUCameraData))),
          static_cast<std::uint32_t>(
              frameInd * getPaddedBufferSize(sizeof(GPUSceneData)))};
      vkCmdBindPipeline(cmd, pipelineBindPoint, material.vkPipeline);

      VkDescriptorSet const descriptorSets[] = {
          _globalDescriptorSet, currentFrameData.objectDataDescriptorSet};
      vkCmdBindDescriptorSets(cmd, pipelineBindPoint, _vkMeshPipelineLayout, 0,
                              2, descriptorSets, 2, offsets);
    }

    VkDeviceSize const bufferOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &bufferOffset);

    vkCmdDraw(cmd, mesh.vertices.size(), 1, 0, i);
  }
}

Material* VulkanEngine::createMaterial(VkPipeline pipeline,
                                       VkPipelineLayout pipelineLayout,
                                       std::string const& name) {
  Material mat;
  mat.vkPipeline = pipeline;
  mat.vkPipelineLayout = pipelineLayout;

  Material& result = (_materials[name] = mat);
  return &result;
}

Material* VulkanEngine::getMaterial(std::string const& name) {
  auto const matIter = _materials.find(name);

  if (matIter == _materials.cend()) {
    return nullptr;
  }

  return &matIter->second;
}

Mesh* VulkanEngine::getMesh(std::string const& name) {
  auto const meshIter = _meshes.find(name);

  if (meshIter == _meshes.cend()) {
    return nullptr;
  }

  return &meshIter->second;
}

AllocatedBuffer
VulkanEngine::createBuffer(std::size_t bufferSize, VkBufferUsageFlags usage,
                           VmaMemoryUsage memoryUsage,
                           VmaAllocationCreateFlags allocationCreateFlags,
                           VmaAllocationInfo* outAllocationInfo) const {
  VkBufferCreateInfo vkBufferCreateInfo = {};
  vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vkBufferCreateInfo.pNext = nullptr;
  vkBufferCreateInfo.size = bufferSize;
  vkBufferCreateInfo.usage = usage;

  VmaAllocationCreateInfo vmaAllocationCreateInfo = {};
  vmaAllocationCreateInfo.usage = memoryUsage;
  vmaAllocationCreateInfo.flags = allocationCreateFlags;

  AllocatedBuffer allocatedBuffer;
  VK_CHECK(vmaCreateBuffer(_vmaAllocator, &vkBufferCreateInfo,
                           &vmaAllocationCreateInfo, &allocatedBuffer.buffer,
                           &allocatedBuffer.allocation, outAllocationInfo));

  return allocatedBuffer;
}

void VulkanEngine::initDescriptors() {
  constexpr std::uint32_t descriptorPoolSize{10};

  std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorPoolSize},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descriptorPoolSize},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorPoolSize}};

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

  VkDescriptorSetLayoutBinding objectDataBinding =
      vkinit::descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         VK_SHADER_STAGE_VERTEX_BIT);

  VkDescriptorSetLayoutCreateInfo objectLayoutCreateInfo =
      vkinit::descriptorSetLayoutCreateInfo(&objectDataBinding, 1);

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
                                 &cameraDescriptorBufferInfo, 1,
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0),
      vkinit::writeDescriptorSet(_globalDescriptorSet,
                                 &sceneDescriptorBufferInfo, 1,
                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1)};

  vkUpdateDescriptorSets(_vkDevice, descriptorSetWrites.size(),
                         descriptorSetWrites.data(), 0, nullptr);

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

    VkDescriptorBufferInfo objectDataDescriptorBufferInfo;
    objectDataDescriptorBufferInfo.buffer =
        _frameDataArray[i].objectDataBuffer.buffer;
    objectDataDescriptorBufferInfo.offset = 0;
    objectDataDescriptorBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writeDescriptorSet =
        vkinit::writeDescriptorSet(_frameDataArray[i].objectDataDescriptorSet,
                                   &objectDataDescriptorBufferInfo, 1,
                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

    vkUpdateDescriptorSets(_vkDevice, 1, &writeDescriptorSet, 0, nullptr);
  }
}

std::size_t VulkanEngine::getPaddedBufferSize(std::size_t originalSize) const {
  std::size_t const minbufferOffset =
      _vkPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
  if (!minbufferOffset)
    return originalSize;

  return (originalSize + minbufferOffset - 1) & (~(minbufferOffset - 1));
}

void VulkanEngine::immediateSubmit(
    std::function<void(VkCommandBuffer cmd)>&& function) {
  VkCommandBufferBeginInfo commandBufferBeginInfo =
      vkinit::commandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(_immediateSubmitContext.vkCommandBuffer,
                                &commandBufferBeginInfo));

  function(_immediateSubmitContext.vkCommandBuffer);

  VK_CHECK(vkEndCommandBuffer(_immediateSubmitContext.vkCommandBuffer));

  VkSubmitInfo submit =
      vkinit::commandBufferSubmitInfo(&_immediateSubmitContext.vkCommandBuffer);

  VK_CHECK(vkQueueSubmit(_vkGraphicsQueue, 1, &submit,
                         _immediateSubmitContext.vkFence));
  vkWaitForFences(_vkDevice, 1, &_immediateSubmitContext.vkFence, VK_TRUE,
                  9999999999);
  vkResetFences(_vkDevice, 1, &_immediateSubmitContext.vkFence);

  VK_CHECK(
      vkResetCommandPool(_vkDevice, _immediateSubmitContext.vkCommandPool, 0));
}

bool VulkanEngine::loadImage(char const* filePath,
                             AllocatedImage& outAllocatedImage) {
  int width, height, texChannels;
  stbi_uc* const pixels =
      stbi_load(filePath, &width, &height, &texChannels, STBI_rgb_alpha);

  if (!pixels) {
    std::cout << "Failed to load image: " << filePath << std::endl;
    return false;
  }

  std::size_t imageOnDeviceSize =
      texChannels * sizeof(std::uint8_t) * width * height;

  std::cout << pixels[imageOnDeviceSize - 1];
  AllocatedBuffer stagingBuffer =
      createBuffer(imageOnDeviceSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO_PREFER_HOST,

                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  void* mappedMemory;
  vmaMapMemory(_vmaAllocator, stagingBuffer.allocation, &mappedMemory);

  std::memcpy(mappedMemory, pixels, imageOnDeviceSize);

  vmaUnmapMemory(_vmaAllocator, stagingBuffer.allocation);

  stbi_image_free(pixels);

  AllocatedImage newImage;
  VkImageUsageFlags const imageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkExtent3D extent = {};
  extent.width = width;
  extent.height = height;
  extent.depth = 1;
  VkFormat const format = VK_FORMAT_R8G8B8A8_SRGB;

  VkImageCreateInfo vkImgCreateInfo =
      vkinit::imageCreateInfo(imageUsageFlags, extent, format);

  VmaAllocationCreateInfo imgAllocationCreateInfo = {};
  imgAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  vmaCreateImage(_vmaAllocator, &vkImgCreateInfo, &imgAllocationCreateInfo,
                 &newImage.vkImage, &newImage.allocation, nullptr);

  immediateSubmit([this, &extent, &newImage,
                   &stagingBuffer](VkCommandBuffer cmd) {
    VkImageMemoryBarrier vkImgBarrierToTransfer = {};
    vkImgBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImgBarrierToTransfer.pNext = nullptr;

    vkImgBarrierToTransfer.srcAccessMask = VK_ACCESS_NONE;
    vkImgBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImgBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImgBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImgBarrierToTransfer.image = newImage.vkImage;

    VkImageSubresourceRange& vkImgSubresourceRangeToTransfer =
        vkImgBarrierToTransfer.subresourceRange;
    vkImgSubresourceRangeToTransfer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToTransfer.baseMipLevel = 0;
    vkImgSubresourceRangeToTransfer.levelCount = 1;
    vkImgSubresourceRangeToTransfer.baseArrayLayer = 0;
    vkImgSubresourceRangeToTransfer.layerCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &vkImgBarrierToTransfer);

    VkBufferImageCopy vkBufferImgCopy = {};
    vkBufferImgCopy.imageExtent = extent;
    vkBufferImgCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBufferImgCopy.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.vkImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &vkBufferImgCopy);

    VkImageMemoryBarrier vkImageBarrierToRead = {};
    vkImageBarrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageBarrierToRead.pNext = nullptr;

    vkImageBarrierToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkImageBarrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkImageBarrierToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageBarrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkImageBarrierToRead.image = newImage.vkImage;
    vkImageBarrierToRead.srcQueueFamilyIndex = 0;
    vkImageBarrierToRead.dstQueueFamilyIndex = _graphicsQueueFamilyIndex;

    VkImageSubresourceRange& vkImgSubresourceRangeToRead =
        vkImageBarrierToRead.subresourceRange;
    vkImgSubresourceRangeToRead.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImgSubresourceRangeToRead.layerCount = 1;
    vkImgSubresourceRangeToRead.levelCount = 1;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &vkImageBarrierToRead);
  });

  outAllocatedImage = newImage;
  _deletionQueue.pushFunction([this, newImage]() {
    vmaDestroyImage(_vmaAllocator, newImage.vkImage, newImage.allocation);
  });

  vmaDestroyBuffer(_vmaAllocator, stagingBuffer.buffer,
                   stagingBuffer.allocation);

  return true;
}
