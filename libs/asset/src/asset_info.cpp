#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/logging.hpp>

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
    int const ret =
        LZ4_decompress_safe(src, dst, srcSize, assetInfo.unpackedSize);

    bool const unpackingSuceeded = ret >= 0;
    if (!unpackingSuceeded) {
      OBS_LOG_ERR("LZ4 decompression failed with code " + std::to_string(ret));
    }
    return unpackingSuceeded;
  }
  default:
    return false;
  }
}

} /*namespace obsidian::asset*/
