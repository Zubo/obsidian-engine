#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace obsidian::vk_rhi {

struct SubpassData {
  VkPipelineBindPoint vkPipelineBindPoint;
  std::vector<VkAttachmentReference> colorAttachmentRefs;
  std::optional<VkAttachmentReference> depthAttachmentRef;
};

class RenderPassBuilder {
public:
  static RenderPassBuilder begin(VkDevice vkDevice);

  RenderPassBuilder& addColorAttachment(VkFormat format,
                                        VkImageLayout finalLayout);

  RenderPassBuilder& addDepthAttachment(VkFormat format);

  RenderPassBuilder& setSubpassPipelineBindPoint(std::size_t subpassInd,
                                                 VkPipelineBindPoint bindPoint);

  RenderPassBuilder& addColorSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& addDepthSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& build(VkRenderPass& outRenderPass);

private:
  RenderPassBuilder() = default;

  VkDevice _vkDevice;
  std::vector<VkAttachmentDescription> _attachmentDescriptions;
  std::vector<SubpassData> _subpasses;
  int _colorAttachmentInd = attachmentIndexNone;
  int _depthAttachmentInd = attachmentIndexNone;

  static constexpr int attachmentIndexNone = -1;
};

} /*namespace obsidian::vk_rhi*/
