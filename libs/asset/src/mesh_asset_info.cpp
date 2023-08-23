#include <asset/asset.hpp>
#include <asset/asset_info.hpp>
#include <asset/mesh_asset_info.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <iostream>

namespace obsidian::asset {

constexpr char const* hasNormalsJsonName = "hasNormals";
constexpr char const* hasUVJsonName = "hasUV";
constexpr char const* hasColorsJsonName = "hasColors";

bool readMeshAssetInfo(Asset const& asset, MeshAssetInfo& outMeshAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMeshAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMeshAssetInfo.compressionMode = json[compressionModeJsonName];
    outMeshAssetInfo.hasNormals = json[hasNormalsJsonName];
    outMeshAssetInfo.hasColors = json[hasColorsJsonName];
    outMeshAssetInfo.hasUV = json[hasUVJsonName];
  } catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
    return false;
  }

  return true;
}

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo, void const* meshData,
                   Asset& outAsset) {
  outAsset.type[0] = 'm';
  outAsset.type[1] = 'e';
  outAsset.type[2] = 's';
  outAsset.type[3] = 'h';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = meshAssetInfo.unpackedSize;
    json[compressionModeJsonName] = meshAssetInfo.compressionMode;
    json[hasNormalsJsonName] = meshAssetInfo.hasNormals;
    json[hasColorsJsonName] = meshAssetInfo.hasColors;
    json[hasUVJsonName] = meshAssetInfo.hasUV;

    outAsset.json = json.dump();

    if (meshAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob.resize(meshAssetInfo.unpackedSize);
      std::memcpy(outAsset.binaryBlob.data(), meshData,
                  outAsset.binaryBlob.size());
    } else if (meshAssetInfo.compressionMode == CompressionMode::LZ4) {
      std::size_t compressedSize =
          LZ4_compressBound(meshAssetInfo.unpackedSize);

      outAsset.binaryBlob.resize(compressedSize);

      LZ4_compress_default(reinterpret_cast<char const*>(meshData),
                           outAsset.binaryBlob.data(),
                           meshAssetInfo.unpackedSize, compressedSize);
    } else {
      std::cout << "Error: Unknown compression mode." << std::endl;
      return false;
    }
  } catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
  }

  return true;
}

std::size_t getVertexSize(MeshAssetInfo const& meshAssetInfo) {
  std::size_t size = 3 * sizeof(float);

  if (meshAssetInfo.hasNormals) {
    size += 3 * sizeof(float);
  }

  if (meshAssetInfo.hasColors) {
    size += 3 * sizeof(float);
  }

  if (meshAssetInfo.hasUV) {
    size += 2 * sizeof(float);
  }

  return size;
}

} /*namespace obsidian::asset*/
