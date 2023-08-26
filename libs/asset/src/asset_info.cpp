#include <obsidian/asset/asset_info.hpp>

#include <lz4.h>

#include <cstring>
#include <tracy/Tracy.hpp>

namespace obsidian::asset {

bool unpackAsset(AssetInfo const& assetInfo, char const* src,
                 std::size_t srcSize, char* dst) {
  switch (assetInfo.compressionMode) {
  case CompressionMode::none: {
    ZoneScopedN("unpackAsset - uncompressed");
    std::memcpy(dst, src, srcSize);
    return true;
  }
  case CompressionMode::LZ4: {
    ZoneScopedN("unpackAsset - LZ4 compression");
    LZ4_decompress_safe(src, dst, srcSize, assetInfo.unpackedSize);
    return true;
  }
  default:
    return false;
  }
}

} /*namespace obsidian::asset*/
