#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>

namespace obsidian::core {

std::size_t getFormatPixelSize(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8G8B8A8_SRGB:
    return 4;
  case TextureFormat::R32G32_SFLOAT:
    return 8;
  default: {
    OBS_LOG_WARN("Unsupported texture format.");
    return 0;
  }
  }
}

TextureFormat getDefaultFormatForChannelCount(std::size_t channelCount) {
  switch (channelCount) {
  case 4:
    return TextureFormat::R8G8B8A8_SRGB;
  default: {
    OBS_LOG_WARN("Unsupported texture format.");
    return TextureFormat::unknown;
  }
  }
}

} /*namespace obsidian::core*/
