#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_rhi.hpp>

#include <cstdint>

namespace obsidian::vk_rhi {

#ifdef USE_VULKAN_DEBUG_FEATURES

static std::atomic<PFN_vkSetDebugUtilsObjectNameEXT> vkSetDebugUtilsObject =
    nullptr;

void initDebugUtils(VkInstance vkInstance) {
  assert(!vkSetDebugUtilsObject);

  vkSetDebugUtilsObject = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
      vkGetInstanceProcAddr(vkInstance, "vkSetDebugUtilsObjectNameEXT"));
}

void setDbgResourceName(VkDevice vkDevice, std::uint64_t objHandle,
                        VkObjectType objType, char const* objName,
                        char const* additionalInfo) {
  assert(vkSetDebugUtilsObject);

  if (!objHandle || !objName) {
    return;
  }

  VkDebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = objType;

  std::string finalName = objName;

  if (additionalInfo) {
    finalName += " AdditionalInfo:";
    finalName += additionalInfo;
  }

  nameInfo.pObjectName = finalName.c_str();
  nameInfo.objectHandle = objHandle;
  nameInfo.pObjectName = objName;

  VK_CHECK((vkSetDebugUtilsObject.load()(vkDevice, &nameInfo)));
}

void nameFramebufferResources(VkDevice vkDevice, Framebuffer const& framebuffer,
                              char const* framebufferName) {

  setDbgResourceName(vkDevice, (std::uint64_t)framebuffer.vkFramebuffer,
                     VK_OBJECT_TYPE_FRAMEBUFFER, framebufferName,
                     "Framebuffer");

  if (framebuffer.colorBufferImage.vkImage != VK_NULL_HANDLE) {
    setDbgResourceName(
        vkDevice, (std::uint64_t)framebuffer.colorBufferImage.vkImage,
        VK_OBJECT_TYPE_IMAGE, framebufferName, "Color attachment image");
  }

  if (framebuffer.colorBufferImageView != VK_NULL_HANDLE) {
    setDbgResourceName(vkDevice,
                       (std::uint64_t)framebuffer.colorBufferImageView,
                       VK_OBJECT_TYPE_IMAGE_VIEW, framebufferName,
                       "Color attachment image view");
  }

  if (framebuffer.depthBufferImage.vkImage != VK_NULL_HANDLE) {
    setDbgResourceName(
        vkDevice, (std::uint64_t)framebuffer.depthBufferImage.vkImage,
        VK_OBJECT_TYPE_IMAGE, framebufferName, "Depth attachment image");
  }

  if (framebuffer.depthBufferImageView != VK_NULL_HANDLE) {
    setDbgResourceName(vkDevice,
                       (std::uint64_t)framebuffer.depthBufferImageView,
                       VK_OBJECT_TYPE_IMAGE_VIEW, framebufferName,
                       "Depth attachment image view");
  }
}

#else

void initDebugUtils(VkInstance vkInstance) {}

void setDbgResourceName(VkDevice vkDevice, std::uint64_t objHandle,
                        VkObjectType objType, char const* objName,
                        char const* additionalInfo) {}

void nameFramebufferResources(VkDevice vkDevice, Framebuffer const& framebuffer,
                              char const* framebufferName) {}
}

#endif

} /*namespace obsidian::vk_rhi */
