#pragma once

#include <cstddef>
#include <cstdint>

namespace obsidian::core {

enum class TextureFormat : std::uint32_t {
  unknown = 0,
  R8G8B8A8_SRGB = 1,
  R32G32_SFLOAT = 2
};

std::size_t getFormatPixelSize(TextureFormat format);
TextureFormat getDefaultFormatForChannelCount(std::size_t channelCount);

} /*namespace obsidian::core*/
