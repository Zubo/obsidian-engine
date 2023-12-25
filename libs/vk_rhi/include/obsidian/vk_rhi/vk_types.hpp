#pragma once

#include <obsidian/core/texture_format.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>

#include <glm/matrix.hpp>
#include <glm/vec4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace obsidian::vk_rhi {

static unsigned int const frameOverlap = 2;

enum class ResourceState { pendingUpload, uploaded, unloaded };

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription2EXT> bindings;
  std::vector<VkVertexInputAttributeDescription2EXT> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
};

struct VkMaterial {
  VkPipeline vkPipelineReuseDepth;
  VkPipeline vkPipelineNoDepthReuse;
  VkPipelineLayout vkPipelineLayout;
  VkDescriptorSet vkDescriptorSet;
  rhi::ResourceRHI resource;
  bool transparent;
  std::vector<rhi::ResourceRHI*> resourceDependencies;
};

struct Shader {
  VkShaderModule vkShaderModule;
  rhi::ResourceRHI resource;
};

struct Mesh;

struct RenderObject {
  Mesh* mesh;
  VkMaterial* material;
  glm::mat4 transformMatrix;
};

struct MeshPushConstants {
  glm::mat4 modelMatrix;
};

struct PostProcessingpushConstants {
  glm::mat3 kernel;
};

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewProj;
};

struct GPUDirectionalLightData {
  glm::mat4 viewProjection;
  glm::vec4 direction;
  glm::vec4 color;
  glm::vec4 intensity;
};

struct GPUSpotlight {
  glm::mat4 viewProjection;
  glm::vec4 direction;
  glm::vec4 position;
  glm::vec4 color;
  glm::vec4 params;
  glm::vec4 attenuation;
};

// used for arrays with std140 alignment
struct PaddedUInt32 {
  std::uint32_t value;

private:
  const std::uint32_t padding[3] = {0, 0, 0};
};

// used as bool scalar with std140 alignment
struct Boolean32 {
  bool value = false;

private:
  const bool padding[3] = {false, false, false};
};

struct GPULightData {
  GPUDirectionalLightData directionalLights[rhi::maxLightsPerDrawPass];
  PaddedUInt32 directionalLightShadowMapIndices[rhi::maxLightsPerDrawPass];
  GPUSpotlight spotlights[rhi::maxLightsPerDrawPass];
  PaddedUInt32 spotlightShadowMapIndices[rhi::maxLightsPerDrawPass];
  std::uint32_t directionalLightCount;
  std::uint32_t spotlightCount;
};

struct GPUSceneData {
  glm::vec4 ambientColor;
};

struct GPUObjectData {
  glm::mat4 modelMat;
};

struct GPULitMaterialData {
  glm::vec4 ambientColor;
  glm::vec4 diffuseColor;
  glm::vec4 specularColor;
  Boolean32 hasDiffuseTex;
  Boolean32 hasNormalMap;
  float shininess;
};

struct GPUUnlitMaterialData {
  glm::vec4 diffuseColor;
  Boolean32 hasDiffuseTex;
};

struct VKDrawCall {
  glm::mat4 model;
  Mesh* mesh;
  VkMaterial* material;
  std::size_t indexBufferInd;
};

struct ShadowPassParams {
  GPUCameraData gpuCameraData;
  int shadowMapIndex;
};

struct ImmediateSubmitContext {
  ~ImmediateSubmitContext();

  VkFence vkFence;
  VkCommandPool vkCommandPool;
  VkCommandBuffer vkCommandBuffer;
  VkDevice device;
  std::uint32_t queueInd;
  bool initialized = false;
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
  rhi::ResourceRHI resource;
};

struct EnvironmentMap {
  AllocatedImage colorImage;
  VkImageView colorImageView;
  AllocatedImage depthImage;
  VkImageView depthImageView;
  std::array<VkImageView, 6> colorAttachmentImageViews;
  std::array<VkImageView, 6> depthAttachmentImageViews;
  std::array<VkFramebuffer, 6> framebuffers;
  glm::vec3 pos;
};

VkFormat getVkTextureFormat(core::TextureFormat format);

GPUCameraData getDirectionalLightCameraData(glm::vec3 direction);
GPUCameraData getSpotlightCameraData(glm::vec3 const& position,
                                     glm::vec3 const& direction,
                                     float cutoffAngleRad);

} /*namespace obsidian::vk_rhi*/
