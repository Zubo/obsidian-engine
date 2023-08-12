#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec4.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
};

struct Material {
  VkPipeline vkPipeline;
  VkPipelineLayout vkPipelineLayout;
  VkDescriptorSet vkDescriptorSet;
};

struct Mesh;

struct RenderObject {
  Mesh* mesh;
  Material* material;
  glm::mat4 transformMatrix;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 renderMatrix;
};

constexpr unsigned int frameOverlap = 2;
constexpr unsigned int maxNumberOfObjects = 10000;

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewProj;
};

struct GPUSceneData {
  glm::vec4 fogColor;
  glm::vec4 fogDistance;
  glm::vec4 ambientColor;
  glm::vec4 sunlightdirection;
  glm::vec4 sunlightcolor;
};

struct GPUObjectData {
  glm::mat4 modelMat;
};

struct FrameData {
  VkSemaphore vkRenderSemaphore;
  VkSemaphore vkPresentSemaphore;
  VkFence vkRenderFence;
  VkCommandPool vkCommandPool;
  VkCommandBuffer vkCommandBuffer;

  AllocatedBuffer objectDataBuffer;
  VkDescriptorSet objectDataDescriptorSet;
};

struct ImmediateSubmitContext {
  VkFence vkFence;
  VkCommandPool vkCommandPool;
  VkCommandBuffer vkCommandBuffer;
};

struct FramebufferImageViews {
  std::vector<VkImageView> vkImageViews;
};

struct AllocatedImage {
  VkImage vkImage;
  VmaAllocation allocation;
};

struct Texture {
  AllocatedImage image;
  VkImageView imageView;
};