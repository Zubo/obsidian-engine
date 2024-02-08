#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace obsidian::asset {

static constexpr std::size_t currentAssetVersion = 0;

enum class AssetType { unknown, mesh, texture, shader, material };

struct AssetMetadata {
  using SizeType = std::size_t;

  char type[4];
  SizeType binaryBlobSize;
  std::uint32_t version;
  std::string json;
};

struct Asset {
  std::optional<AssetMetadata> metadata;
  std::vector<char> binaryBlob;
  bool isLoaded = false;
};

AssetType getAssetType(char const typeStr[4]);

} /*namespace obsidian::asset*/
