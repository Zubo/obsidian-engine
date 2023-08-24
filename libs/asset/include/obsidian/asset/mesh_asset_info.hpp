#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <cstddef>

namespace obsidian::asset {

struct Asset;

struct MeshAssetInfo : public AssetInfo {
  bool hasNormals;
  bool hasColors;
  bool hasUV;
};

bool readMeshAssetInfo(Asset const& asset, MeshAssetInfo& outMeshAssetInfo);

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo, void const* meshData,
                   Asset& outAsset);

std::size_t getVertexSize(MeshAssetInfo const& meshAssetInfo);

} // namespace obsidian::asset
