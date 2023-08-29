#include <obsidian/vk_rhi/vk_types.hpp>

namespace obsidian::vk_rhi {

VkFormat getVkTextureFormat(core::TextureFormat format) {
  switch (format) {
  case core::TextureFormat::R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  default:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

} /*namespace obsidian::vk_rhi*/
