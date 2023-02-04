#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vk_engine.hpp>
#include <vk_initializers.hpp>

#include <cstdint>
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
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WindowFlags windowFlags{static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN)};
  Window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, WindowExtent.width,
                            WindowExtent.height, windowFlags);

  initVulkan();

  initSwapchain();

  initCommands();

  initDefaultRenderPass();

  initFramebuffers();

  initSyncStructures();

  initPipelines();

  IsInitialized = true;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool shouldQuit = false;

  while (!shouldQuit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        shouldQuit = true;
    }

    draw();
  }
}

void VulkanEngine::cleanup() {
  if (IsInitialized) {
    vkDestroyCommandPool(_vkDevice, _vkCommandPool, nullptr);
    vkDestroySwapchainKHR(_vkDevice, _vkSwapchain, nullptr);

    vkDestroyRenderPass(_vkDevice, _vkRenderPass, nullptr);

    for (int i = 0; i < _vkSwapchainImageViews.size(); ++i) {
      vkDestroyFramebuffer(_vkDevice, _vkFramebuffers.at(i), nullptr);
      vkDestroyImageView(_vkDevice, _vkSwapchainImageViews[i], nullptr);
    }

    vkDestroyDevice(_vkDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);
    vkDestroyInstance(_vkInstance, nullptr);
    SDL_DestroyWindow(Window);

    IsInitialized = false;
  }
}

void VulkanEngine::draw() {
  constexpr std::uint64_t timeoutNanoseconds = 1000000000;
  VK_CHECK(
      vkWaitForFences(_vkDevice, 1, &_vkRenderFence, true, timeoutNanoseconds));
  VK_CHECK(vkResetFences(_vkDevice, 1, &_vkRenderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(_vkDevice, _vkSwapchain, timeoutNanoseconds,
                                 _vkPresentSemaphore, VK_NULL_HANDLE,
                                 &swapchainImageIndex));

  VK_CHECK(vkResetCommandBuffer(_vkCommandBufferMain, 0));

  VkCommandBuffer cmd = _vkCommandBufferMain;

  VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {};
  vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkCommandBufferBeginInfo.pNext = nullptr;
  vkCommandBufferBeginInfo.pInheritanceInfo = nullptr;
  vkCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &vkCommandBufferBeginInfo));

  VkClearValue clearValue;
  float flash = std::abs(std::sin(_frameNumber / 10.0f));
  clearValue.color = {{0.0f, 0.0f, flash, 1.0f}};

  VkRenderPassBeginInfo vkRenderPassBeginInfo = {};
  vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  vkRenderPassBeginInfo.pNext = nullptr;
  vkRenderPassBeginInfo.renderPass = _vkRenderPass;
  vkRenderPassBeginInfo.framebuffer = _vkFramebuffers[swapchainImageIndex];
  vkRenderPassBeginInfo.renderArea.offset = {0, 0};
  vkRenderPassBeginInfo.renderArea.extent = WindowExtent;
  vkRenderPassBeginInfo.clearValueCount = 1;
  vkRenderPassBeginInfo.pClearValues = &clearValue;

  vkCmdBeginRenderPass(cmd, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vkTrianglePipeline);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo vkSubmitInfo = {};
  vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  vkSubmitInfo.pNext = nullptr;

  VkPipelineStageFlags const vkPipelineStageFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  vkSubmitInfo.pWaitDstStageMask = &vkPipelineStageFlags;
  vkSubmitInfo.waitSemaphoreCount = 1;
  vkSubmitInfo.pWaitSemaphores = &_vkPresentSemaphore;

  vkSubmitInfo.signalSemaphoreCount = 1;
  vkSubmitInfo.pSignalSemaphores = &_vkRenderSemaphore;

  vkSubmitInfo.commandBufferCount = 1;
  vkSubmitInfo.pCommandBuffers = &cmd;

  VK_CHECK(vkQueueSubmit(_vkGraphicsQueue, 1, &vkSubmitInfo, _vkRenderFence));

  VkPresentInfoKHR vkPresentInfo = {};
  vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  vkPresentInfo.pNext = nullptr;

  vkPresentInfo.pSwapchains = &_vkSwapchain;
  vkPresentInfo.swapchainCount = 1;

  vkPresentInfo.waitSemaphoreCount = 1;
  vkPresentInfo.pWaitSemaphores = &_vkRenderSemaphore;

  vkPresentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_vkGraphicsQueue, &vkPresentInfo));

  ++_frameNumber;
}

void VulkanEngine::initVulkan() {
  vkb::InstanceBuilder builder;
  auto const builderReturn = builder.set_app_name("VKGuide tutorial")
                                 .request_validation_layers(true)
                                 .require_api_version(1, 1, 0)
                                 .use_default_debug_messenger()
                                 .build();

  vkb::Instance vkbInstance = builderReturn.value();

  _vkInstance = vkbInstance.instance;
  _vkDebugMessenger = vkbInstance.debug_messenger;

  SDL_Vulkan_CreateSurface(Window, _vkInstance, &_vkSurface);
  vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
  vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector.set_minimum_version(1, 1)
                                              .set_surface(_vkSurface)
                                              .select()
                                              .value();

  vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
  vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

  _vkDevice = vkbDevice.device;
  _vkPhysicalDevice = vkbPhysicalDevice.physical_device;

  _vkGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamilyIndex =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
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
  _vkSwapchainImageViews = vkbSwapchain.get_image_views().value();

  _vkSwapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::initCommands() {
  VkCommandPoolCreateInfo vkCommandPoolCreateInfo =
      vkinit::commandPoolCreateInfo(
          _graphicsQueueFamilyIndex,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VK_CHECK(vkCreateCommandPool(_vkDevice, &vkCommandPoolCreateInfo, nullptr,
                               &_vkCommandPool));

  constexpr std::uint32_t commandPoolCount = 1;
  VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo =
      vkinit::commandBufferAllocateInfo(_vkCommandPool, commandPoolCount,
                                        VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  VK_CHECK(vkAllocateCommandBuffers(_vkDevice, &vkCommandBufferAllocateInfo,
                                    &_vkCommandBufferMain));
}

void VulkanEngine::initDefaultRenderPass() {
  VkAttachmentDescription vkColorAttachment = {};
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

  VkSubpassDescription vkSubpassDescription = {};
  vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkSubpassDescription.colorAttachmentCount = 1;
  vkSubpassDescription.pColorAttachments = &vkColorAttachmentReference;

  VkRenderPassCreateInfo vkRenderPassCreateInfo = {};
  vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  vkRenderPassCreateInfo.pNext = nullptr;

  vkRenderPassCreateInfo.attachmentCount = 1;
  vkRenderPassCreateInfo.pAttachments = &vkColorAttachment;

  vkRenderPassCreateInfo.subpassCount = 1;
  vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;

  vkCreateRenderPass(_vkDevice, &vkRenderPassCreateInfo, nullptr,
                     &_vkRenderPass);
}

void VulkanEngine::initFramebuffers() {
  std::size_t const swapchainImageCount = _vkSwapchainImageViews.size();
  VkFramebufferCreateInfo vkFramebufferCreateInfo = {};
  vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  vkFramebufferCreateInfo.pNext = nullptr;
  vkFramebufferCreateInfo.renderPass = _vkRenderPass;
  vkFramebufferCreateInfo.attachmentCount = 1;
  vkFramebufferCreateInfo.width = WindowExtent.width;
  vkFramebufferCreateInfo.height = WindowExtent.height;
  vkFramebufferCreateInfo.layers = 1;

  _vkFramebuffers.resize(swapchainImageCount);
  for (int i = 0; i < swapchainImageCount; ++i) {
    vkFramebufferCreateInfo.pAttachments = &_vkSwapchainImageViews.at(i);
    VK_CHECK(vkCreateFramebuffer(_vkDevice, &vkFramebufferCreateInfo, nullptr,
                                 &_vkFramebuffers[i]));
  }
}

void VulkanEngine::initSyncStructures() {
  VkFenceCreateInfo vkFenceCreateInfo = {};
  vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkFenceCreateInfo.pNext = nullptr;
  vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(
      vkCreateFence(_vkDevice, &vkFenceCreateInfo, nullptr, &_vkRenderFence));

  VkSemaphoreCreateInfo vkSemaphoreCreateInfo = {};
  vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkSemaphoreCreateInfo.pNext = nullptr;
  vkSemaphoreCreateInfo.flags = 0;
  VK_CHECK(vkCreateSemaphore(_vkDevice, &vkSemaphoreCreateInfo, nullptr,
                             &_vkRenderSemaphore));
  VK_CHECK(vkCreateSemaphore(_vkDevice, &vkSemaphoreCreateInfo, nullptr,
                             &_vkPresentSemaphore));
}

bool VulkanEngine::loadShaderModule(char const *filePath,
                                    VkShaderModule *outShaderModule) {
  std::ifstream file{filePath, std::ios::ate | std::ios::binary};

  if (!file.is_open()) {
    return false;
  }

  std::size_t const fileSize = static_cast<std::size_t>(file.tellg());
  std::vector<std::uint32_t> buffer((fileSize + sizeof(std::uint32_t) - 1) /
                                    sizeof(std::uint32_t));

  file.seekg(0);

  file.read(reinterpret_cast<char *>(buffer.data()), fileSize);

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
  VkShaderModule triangleVertShader;

  if (!loadShaderModule("shaders/triangle.vert.spv", &triangleVertShader)) {
    std::cout << "Error when building the triangle vertex shader module."
              << std::endl;
  } else {
    std::cout << "Triangle vertex shader successfully loaded." << std::endl;
  }

  VkShaderModule triangleFragShader;

  if (!loadShaderModule("shaders/triangle.frag.spv", &triangleFragShader)) {
    std::cout << "Error when building the triangle fragment shader module."
              << std::endl;
  } else {
    std::cout << "Triangle fragment shader successfully loaded." << std::endl;
  }

  VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo =
      vkinit::pipelineLayoutCreateInfo();
  VK_CHECK(vkCreatePipelineLayout(_vkDevice, &vkPipelineLayoutCreateInfo,
                                  nullptr, &_vkTrianglePipelineLayout));

  PipelineBuilder pipelineBuilder;

  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            triangleVertShader));
  pipelineBuilder._vkShaderStageCreateInfo.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            triangleFragShader));
  pipelineBuilder._vkVertexInputInfo = vkinit::vertexInputStateCreateInfo();
  pipelineBuilder._vkInputAssemblyCreateInfo =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

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
  pipelineBuilder._vkTrianglePipelineLayout = _vkTrianglePipelineLayout;

  _vkTrianglePipeline = pipelineBuilder.buildPipeline(_vkDevice, _vkRenderPass);
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
  graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
  graphicsPipelineCreateInfo.pDynamicState = nullptr;
  graphicsPipelineCreateInfo.layout = _vkTrianglePipelineLayout;
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
