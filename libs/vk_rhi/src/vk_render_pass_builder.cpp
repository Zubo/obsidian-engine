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
RenderPassBuilder::addColorAttachment(VkFormat format,
                                      VkImageLayout finalLayout) {
  assert(_colorAttachmentInd == attachmentIndexNone);

  _colorAttachmentInd = _attachmentDescriptions.size();
  _attachmentDescriptions.push_back(
      vkinit::colorAttachmentDescription(format, finalLayout));

  return *this;
}

RenderPassBuilder& RenderPassBuilder::addDepthAttachment(VkFormat format,
                                                         bool storeResult) {
  assert(_depthAttachmentInd == attachmentIndexNone);

  _depthAttachmentInd = _attachmentDescriptions.size();

  VkAttachmentDescription depthAttachmentDescription =
      vkinit::depthAttachmentDescription(format);

  depthAttachmentDescription.loadOp =
      storeResult ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  depthAttachmentDescription.storeOp = storeResult
                                           ? VK_ATTACHMENT_STORE_OP_STORE
                                           : VK_ATTACHMENT_STORE_OP_DONT_CARE;

  if (!storeResult) {
    depthAttachmentDescription.initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  }

  _attachmentDescriptions.push_back(depthAttachmentDescription);

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
RenderPassBuilder::addColorSubpassReference(std::size_t subpassInd,
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
RenderPassBuilder::addDepthSubpassReference(std::size_t subpassInd,
                                            VkImageLayout layout) {
  assert(_depthAttachmentInd != attachmentIndexNone);
  if (subpassInd >= _subpasses.size()) {
    _subpasses.resize(subpassInd + 1);
  }

  SubpassData& subpass = _subpasses[subpassInd];
  assert(!subpass.depthAttachmentRef);

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

  return *this;
}
