#include <obsidian/asset/asset.hpp>

#include <cstring>

namespace obsidian::asset {

AssetType getAssetType(char const typeStr[4]) {
  if (std::strncmp(typeStr, "MESH", 4) == 0) {
    return AssetType::mesh;
  } else if (std::strncmp(typeStr, "TEXI", 4) == 0) {
    return AssetType::texture;
  }

  return AssetType::unknown;
}

} /*namespace obsidian::asset*/
