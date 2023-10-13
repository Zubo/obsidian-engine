#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_framebuffer.hpp>
#include <obsidian/vk_rhi/vk_initializers.hpp>

using namespace obsidian::vk_rhi;

Framebuffer
RenderPass::generateFramebuffer(VmaAllocator vmaAllocator, VkExtent2D extent,
                                AttachmentImageUsage attachmentUsages,
                                VkImageView overrideColorImageView) {
  Framebuffer outFramebuffer = {};

  VkExtent3D const extent3D = {extent.width, extent.height, 1};

  std::vector<VkImageView> attachmentImageViews;

  VmaAllocationCreateInfo allocationCreateInfo = {};
  allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (overrideColorImageView) {
    outFramebuffer.colorBufferImageView = overrideColorImageView;
    attachmentImageViews.push_back(outFramebuffer.colorBufferImageView);
  } else if (colorAttachmentFormat) {
    VkImageCreateInfo const colorImageCreateInfo = vkinit::imageCreateInfo(
        attachmentUsages.colorImageUsage, extent3D, *colorAttachmentFormat);

    VK_CHECK(vmaCreateImage(
        vmaAllocator, &colorImageCreateInfo, &allocationCreateInfo,
        &outFramebuffer.colorBufferImage.vkImage,
        &outFramebuffer.colorBufferImage.allocation, nullptr));

    VkImageViewCreateInfo colorImageViewCreateInfo =
        vkinit::imageViewCreateInfo(outFramebuffer.colorBufferImage.vkImage,
                                    *colorAttachmentFormat,
                                    VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(vkDevice, &colorImageViewCreateInfo, nullptr,
                               &outFramebuffer.colorBufferImageView));

    attachmentImageViews.push_back(outFramebuffer.colorBufferImageView);
  }

  if (depthAttachmentFormat) {
    VkImageCreateInfo const depthImageCreateInfo = vkinit::imageCreateInfo(
        attachmentUsages.depthImageUsage, extent3D, *depthAttachmentFormat);

    VK_CHECK(vmaCreateImage(
        vmaAllocator, &depthImageCreateInfo, &allocationCreateInfo,
        &outFramebuffer.depthBufferImage.vkImage,
        &outFramebuffer.depthBufferImage.allocation, nullptr));

    VkImageViewCreateInfo depthImageViewCreateInfo =
        vkinit::imageViewCreateInfo(outFramebuffer.depthBufferImage.vkImage,
                                    *depthAttachmentFormat,
                                    VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(vkDevice, &depthImageViewCreateInfo, nullptr,
                               &outFramebuffer.depthBufferImageView));

    attachmentImageViews.push_back(outFramebuffer.depthBufferImageView);
  }

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.pNext = nullptr;

  framebufferCreateInfo.renderPass = vkRenderPass;
  framebufferCreateInfo.attachmentCount = attachmentImageViews.size();
  framebufferCreateInfo.pAttachments = attachmentImageViews.data();
  framebufferCreateInfo.width = extent3D.width;
  framebufferCreateInfo.height = extent3D.height;
  framebufferCreateInfo.layers = extent3D.depth;

  VK_CHECK(vkCreateFramebuffer(vkDevice, &framebufferCreateInfo, nullptr,
                               &outFramebuffer.vkFramebuffer));

  return outFramebuffer;
}
