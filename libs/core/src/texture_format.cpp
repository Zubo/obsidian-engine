#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>

namespace obsidian::core {

std::size_t getFormatPixelSize(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8G8B8:
    return 3;
  case TextureFormat::R8G8B8A8:
    return 4;
  default: {
    OBS_LOG_WARN("Unknown texture format.");
    return 0;
  }
  }
}

} /*namespace obsidian::core*/
