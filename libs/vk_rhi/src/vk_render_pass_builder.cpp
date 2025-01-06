#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>
#include <obsidian/vk_rhi/vk_render_pass_builder.hpp>

#include <cassert>

using namespace obsidian::vk_rhi;

RenderPassBuilder RenderPassBuilder::begin(VkDevice vkDevice) {
  RenderPassBuilder builder;
  builder._vkDevice = vkDevice;
  return builder;
}

RenderPassBuilder&
RenderPassBuilder::setSampleCount(VkSampleCountFlagBits sampleCount) {
  _sampleCount = sampleCount;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::setColorAttachment(
    VkFormat format, VkImageLayout finalLayout, bool storeResult) {
  if (_colorAttachmentInd == attachmentIndexNone) {
    _colorAttachmentInd = _attachmentDescriptions.size();
    _attachmentDescriptions.emplace_back();
  }

  VkAttachmentDescription& colorAttachmentDescription =
      _attachmentDescriptions[_colorAttachmentInd];

  colorAttachmentDescription =
      vkinit::colorAttachmentDescription(format, finalLayout, _sampleCount);
  colorAttachmentDescription.storeOp = storeResult
                                           ? colorAttachmentDescription.storeOp
                                           : VK_ATTACHMENT_STORE_OP_DONT_CARE;

  return *this;
}

RenderPassBuilder& RenderPassBuilder::setDepthAttachment(VkFormat format,
                                                         bool storeResult) {
  if (_depthAttachmentInd == attachmentIndexNone) {
    _depthAttachmentInd = _attachmentDescriptions.size();
    _attachmentDescriptions.push_back(
        vkinit::depthAttachmentDescription(format, _sampleCount));
  }

  VkAttachmentDescription& depthAttachmentDescription =
      _attachmentDescriptions[_depthAttachmentInd];

  depthAttachmentDescription.loadOp =
      storeResult ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  depthAttachmentDescription.storeOp = storeResult
                                           ? VK_ATTACHMENT_STORE_OP_STORE
                                           : VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachmentDescription.samples = _sampleCount;

  if (storeResult) {
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  } else {
    depthAttachmentDescription.initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthAttachmentDescription.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  }

  return *this;
}

RenderPassBuilder&
RenderPassBuilder::setResolveAttachment(VkFormat format,
                                        VkImageLayout finalLayout) {
  if (_resolveAttachmentInd == attachmentIndexNone) {
    _resolveAttachmentInd = _attachmentDescriptions.size();
    _attachmentDescriptions.push_back(vkinit::colorAttachmentDescription(
        format, finalLayout, VK_SAMPLE_COUNT_1_BIT));
  }

  return *this;
}

RenderPassBuilder&
RenderPassBuilder::setSubpassPipelineBindPoint(std::size_t subpassInd,
                                               VkPipelineBindPoint bindPoint) {
  if (subpassInd >= _subpasses.size()) {
    _subpasses.resize(subpassInd + 1);
  }

  _subpasses[subpassInd].vkPipelineBindPoint = bindPoint;

  return *this;
}

RenderPassBuilder&
RenderPassBuilder::setColorSubpassReference(std::size_t subpassInd,
                                            VkImageLayout layout) {
  assert(_colorAttachmentInd != attachmentIndexNone);

  if (subpassInd >= _subpasses.size()) {
    _subpasses.resize(subpassInd + 1);
  }

  SubpassData& subpass = _subpasses[subpassInd];
  VkAttachmentReference& colorAttachmentRef =
      subpass.colorAttachmentRefs.emplace_back();

  colorAttachmentRef.attachment = _colorAttachmentInd;
  colorAttachmentRef.layout = layout;

  return *this;
}

RenderPassBuilder&
RenderPassBuilder::setResolveSubpassReference(std::size_t subpassInd,
                                              VkImageLayout layout) {
  assert(_resolveAttachmentInd != attachmentIndexNone);

  if (subpassInd >= _subpasses.size()) {
    _subpasses.resize(subpassInd + 1);
  }

  SubpassData& subpass = _subpasses[subpassInd];

  VkAttachmentReference& resolveAttachmentRef =
      subpass.resolveAttachmentRefs.emplace_back();

  resolveAttachmentRef.attachment = _resolveAttachmentInd;
  resolveAttachmentRef.layout = layout;

  return *this;
}

RenderPassBuilder&
RenderPassBuilder::setDepthSubpassReference(std::size_t subpassInd,
                                            VkImageLayout layout) {
  assert(_depthAttachmentInd != attachmentIndexNone);
  if (subpassInd >= _subpasses.size()) {
    _subpasses.resize(subpassInd + 1);
  }

  SubpassData& subpass = _subpasses[subpassInd];
  subpass.depthAttachmentRef.emplace();
  subpass.depthAttachmentRef->attachment = _depthAttachmentInd;
  subpass.depthAttachmentRef->layout = layout;

  return *this;
}

RenderPassBuilder& RenderPassBuilder::build(RenderPass& outRenderPass) {
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.pNext = nullptr;

  renderPassCreateInfo.attachmentCount = _attachmentDescriptions.size();
  renderPassCreateInfo.pAttachments = _attachmentDescriptions.data();

  renderPassCreateInfo.subpassCount = _subpasses.size();

  std::vector<VkSubpassDescription> subpassDescriptions;

  for (SubpassData const& subpassData : _subpasses) {
    VkSubpassDescription& subpassDescription =
        subpassDescriptions.emplace_back();
    subpassDescription = {};

    subpassDescription.pipelineBindPoint = subpassData.vkPipelineBindPoint;

    subpassDescription.colorAttachmentCount =
        subpassData.colorAttachmentRefs.size();
    subpassDescription.pColorAttachments =
        subpassData.colorAttachmentRefs.data();

    if (subpassData.resolveAttachmentRefs.size()) {
      assert(subpassData.resolveAttachmentRefs.size() ==
             subpassData.colorAttachmentRefs.size());
      subpassDescription.pResolveAttachments =
          subpassData.resolveAttachmentRefs.data();
    }

    if (subpassData.depthAttachmentRef) {
      subpassDescription.pDepthStencilAttachment =
          &subpassData.depthAttachmentRef.value();
    }
  }

  renderPassCreateInfo.pSubpasses = subpassDescriptions.data();

  VK_CHECK(vkCreateRenderPass(_vkDevice, &renderPassCreateInfo, nullptr,
                              &outRenderPass.vkRenderPass));

  outRenderPass.vkDevice = _vkDevice;

  if (_colorAttachmentInd != attachmentIndexNone) {
    outRenderPass.colorAttachmentFormat =
        _attachmentDescriptions[_colorAttachmentInd].format;
  }

  if (_depthAttachmentInd != attachmentIndexNone) {
    outRenderPass.depthAttachmentFormat =
        _attachmentDescriptions[_depthAttachmentInd].format;
  }

  if (_resolveAttachmentInd != attachmentIndexNone) {
    outRenderPass.resolveAttachmentFormat =
        _attachmentDescriptions[_resolveAttachmentInd].format;
  }

  outRenderPass.sampleCount = _sampleCount;

  return *this;
}
