#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>

#include <string>
#include <tracy/Tracy.hpp>

namespace obsidian::asset {

bool compress(std::span<char const> src, std::vector<char>& outDst) {
  ZoneScoped;

  std::size_t maxCompressedSize = LZ4_compressBound(src.size());

  outDst.resize(maxCompressedSize);

  int actualCompressedSize = LZ4_compress_default(
      src.data(), outDst.data(), src.size(), maxCompressedSize);

  if (actualCompressedSize < 0) {
    OBS_LOG_ERR("LZ4 compression failed with error code " +
                std::to_string(actualCompressedSize));

    return false;
  }

  outDst.resize(actualCompressedSize);

  return true;
}

} // namespace obsidian::asset
