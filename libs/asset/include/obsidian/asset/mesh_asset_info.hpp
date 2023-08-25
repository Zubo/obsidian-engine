#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <cstddef>
#include <vector>

namespace obsidian::asset {

struct Asset;

struct MeshAssetInfo : public AssetInfo {
  std::size_t vertexCount;
  bool hasNormals;
  bool hasColors;
  bool hasUV;
};

bool readMeshAssetInfo(Asset const& asset, MeshAssetInfo& outMeshAssetInfo);

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo,
                   std::vector<char> meshData, Asset& outAsset);

std::size_t getVertexSize(MeshAssetInfo const& meshAssetInfo);

} // namespace obsidian::asset
