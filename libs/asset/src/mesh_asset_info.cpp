#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <tracy/Tracy.hpp>

namespace obsidian::asset {

constexpr char const* vertexCountJsonName = "vertexCount";
constexpr char const* vertexBufferSizeJsonName = "vertexBufferSize";
constexpr char const* indexCountJsonName = "indexCount";
constexpr char const* indexBufferSizeJsonName = "indexBufferSize";
constexpr char const* hasNormalsJsonName = "hasNormals";
constexpr char const* hasUVJsonName = "hasUV";
constexpr char const* hasColorsJsonName = "hasColors";

bool readMeshAssetInfo(Asset const& asset, MeshAssetInfo& outMeshAssetInfo) {
  ZoneScoped;
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMeshAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMeshAssetInfo.compressionMode = json[compressionModeJsonName];
    outMeshAssetInfo.vertexCount = json[vertexCountJsonName];
    outMeshAssetInfo.vertexBufferSize = json[vertexBufferSizeJsonName];
    outMeshAssetInfo.indexCount = json[indexCountJsonName];
    outMeshAssetInfo.indexBufferSize = json[indexBufferSizeJsonName];
    outMeshAssetInfo.hasNormals = json[hasNormalsJsonName];
    outMeshAssetInfo.hasColors = json[hasColorsJsonName];
    outMeshAssetInfo.hasUV = json[hasUVJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo,
                   std::vector<char> meshData, Asset& outAsset) {
  ZoneScoped;
  outAsset.type[0] = 'm';
  outAsset.type[1] = 'e';
  outAsset.type[2] = 's';
  outAsset.type[3] = 'h';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = meshAssetInfo.unpackedSize;
    json[compressionModeJsonName] = meshAssetInfo.compressionMode;
    json[vertexCountJsonName] = meshAssetInfo.vertexCount;
    json[vertexBufferSizeJsonName] = meshAssetInfo.vertexBufferSize;
    json[indexCountJsonName] = meshAssetInfo.indexCount;
    json[indexBufferSizeJsonName] = meshAssetInfo.indexBufferSize;
    json[hasNormalsJsonName] = meshAssetInfo.hasNormals;
    json[hasColorsJsonName] = meshAssetInfo.hasColors;
    json[hasUVJsonName] = meshAssetInfo.hasUV;

    outAsset.json = json.dump();

    if (meshAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(meshData);
    } else if (meshAssetInfo.compressionMode == CompressionMode::LZ4) {
      std::size_t compressedBufferSize =
          LZ4_compressBound(meshAssetInfo.unpackedSize);

      outAsset.binaryBlob.resize(compressedBufferSize);

      int const ret = LZ4_compress_default(
          meshData.data(), outAsset.binaryBlob.data(),
          meshAssetInfo.unpackedSize, compressedBufferSize);

      if (ret < 0) {
        OBS_LOG_ERR("LZ4 compression failed with error code " +
                    std::to_string(ret));
        return false;
      }

      outAsset.binaryBlob.resize(ret);
    } else {
      OBS_LOG_ERR("Error: Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
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
