#pragma once

#include <cstddef>
#include <cstdint>

namespace obsidian::asset {

constexpr char const* unpackedSizeJsonName = "unpackedSize";
constexpr char const* compressionModeJsonName = "compressionMode";

enum class CompressionMode : std::uint32_t { none = 0, LZ4 = 1 };

struct AssetInfo {
  std::size_t unpackedSize;
  CompressionMode compressionMode;
};

bool unpackAsset(AssetInfo const& assetInfo, char const* src,
                 std::size_t srcSize, char* dst);

} /*namespace obsidian::asset*/
