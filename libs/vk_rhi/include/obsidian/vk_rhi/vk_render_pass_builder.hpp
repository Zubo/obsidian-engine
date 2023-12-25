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
  std::optional<VkAttachmentReference> depthAttachmentRef;
};

class RenderPassBuilder {
public:
  static RenderPassBuilder begin(VkDevice vkDevice);

  RenderPassBuilder& setColorAttachment(VkFormat format,
                                        VkImageLayout finalLayout);

  RenderPassBuilder& setDepthAttachment(VkFormat format,
                                        bool storeResult = true);

  RenderPassBuilder& setSubpassPipelineBindPoint(std::size_t subpassInd,
                                                 VkPipelineBindPoint bindPoint);

  RenderPassBuilder& setColorSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& setDepthSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout);

  RenderPassBuilder& build(RenderPass& outRenderPass);

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
