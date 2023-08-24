#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace obsidian::asset {

static constexpr std::size_t currentAssetVersion = 0;

enum class AssetType { unknown, mesh, texture };

struct Asset {
  using SizeType = std::size_t;

  char type[4];
  std::uint32_t version;
  std::string json;
  std::vector<char> binaryBlob;
};

AssetType getAssetType(char const typeStr[4]);

} /*namespace obsidian::asset*/
