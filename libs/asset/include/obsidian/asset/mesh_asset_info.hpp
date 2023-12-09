#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct MeshAssetInfo : public AssetInfo {
  std::size_t vertexCount;
  std::size_t vertexBufferSize;
  std::size_t indexCount;
  std::vector<std::size_t> indexBufferSizes;
  std::vector<std::string> defaultMatRelativePaths;
  bool hasNormals;
  bool hasColors;
  bool hasUV;
  bool hasTangents;
};

bool readMeshAssetInfo(AssetMetadata const& assetMetadata,
                       MeshAssetInfo& outMeshAssetInfo);

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo,
                   std::vector<char> meshData, Asset& outAsset);

} // namespace obsidian::asset
