#include <obsidian/asset/asset.hpp>

#include <cstring>

namespace obsidian::asset {

AssetType getAssetType(char const typeStr[4]) {
  if (std::strncmp(typeStr, "mesh", 4) == 0) {
    return AssetType::mesh;
  } else if (std::strncmp(typeStr, "texi", 4) == 0) {
    return AssetType::texture;
  } else if (std::strncmp(typeStr, "shdr", 4) == 0) {
    return AssetType::shader;
  } else if (std::strncmp(typeStr, "matl", 4) == 0) {
    return AssetType::material;
  }

  return AssetType::unknown;
}

} /*namespace obsidian::asset*/
