#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace obsidian::vk_rhi {

struct RenderPass;

struct SubpassData {
  VkPipelineBindPoint vkPipelineBindPoint;
  std::vector<VkAttachmentReference> colorAttachmentRefs;
  std::vector<VkAttachmentReference> resolveAttachmentRefs;
  std::optional<VkAttachmentReference> depthAttachmentRef;
};

class RenderPassBuilder {
public:
  static RenderPassBuilder begin(VkDevice vkDevice);

  RenderPassBuilder& setSampleCount(VkSampleCountFlagBits sampleCount);

  RenderPassBuilder& setColorAttachment(VkFormat format,
                                        VkImageLayout finalLayout,
                                        bool storeResult = true);

  RenderPassBuilder& setDepthAttachment(VkFormat format,
                                        bool storeResult = true);

  RenderPassBuilder& setResolveAttachment(VkFormat format,
                                          VkImageLayout finalLayout);

  RenderPassBuilder& setSubpassPipelineBindPoint(std::size_t subpassInd,
                                                 VkPipelineBindPoint bindPoint);

  RenderPassBuilder& setColorSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& setDepthSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& setResolveSubpassReference(std::size_t subpassInd,
                                                VkImageLayout layout);

  RenderPassBuilder& build(RenderPass& outRenderPass);

private:
  RenderPassBuilder() = default;

  VkDevice _vkDevice;
  std::vector<VkAttachmentDescription> _attachmentDescriptions;
  std::vector<SubpassData> _subpasses;
  int _colorAttachmentInd = attachmentIndexNone;
  int _depthAttachmentInd = attachmentIndexNone;
  int _resolveAttachmentInd = attachmentIndexNone;
  VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;

  static constexpr int attachmentIndexNone = -1;
};

} /*namespace obsidian::vk_rhi*/
