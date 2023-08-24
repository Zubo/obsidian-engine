#pragma once

#include <cstddef>
#include <cstdint>

namespace obsidian::core {

enum class TextureFormat : std::uint32_t {
  unknown = 0,
  R8G8B8 = 1,
  R8G8B8A8 = 2
};

std::size_t getFormatPixelSize(TextureFormat format);

} /*namespace obsidian::core*/
