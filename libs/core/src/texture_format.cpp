#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>

namespace obsidian::core {

std::size_t getFormatPixelSize(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8G8B8A8_SRGB:
  case TextureFormat::R8G8B8A8_LINEAR:
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
  case 3:
    return TextureFormat::R8G8B8A8_SRGB;
  default: {
    OBS_LOG_WARN("Unsupported texture format.");
    return TextureFormat::unknown;
  }
  }
}

bool isFormatLinear(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8G8B8A8_SRGB:
    return false;
  case TextureFormat::R8G8B8A8_LINEAR:
  case TextureFormat::R32G32_SFLOAT:
    return true;
  default:
    OBS_LOG_WARN("Unknown texture format");
    return false;
  }
}

std::size_t numberOfNonLinearChannels(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8G8B8A8_SRGB:
    return 3;
  case TextureFormat::R8G8B8A8_LINEAR:
    return 0;
  case TextureFormat::R32G32_SFLOAT:
    return 0;
  default:
    OBS_LOG_WARN("Unknown texture format");
    return false;
  }
}

} /*namespace obsidian::core*/
