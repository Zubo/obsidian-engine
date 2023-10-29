#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>
#include <cstring>
#include <exception>

namespace obsidian::asset {

constexpr char const* vertexCountJsonName = "vertexCount";
constexpr char const* vertexBufferSizeJsonName = "vertexBufferSize";
constexpr char const* indexBufferSizesJsonName = "indexBufferSizes";
constexpr char const* indexCountJsonName = "indexCount";
constexpr char const* hasNormalsJsonName = "hasNormals";
constexpr char const* hasUVJsonName = "hasUV";
constexpr char const* hasColorsJsonName = "hasColors";
constexpr char const* defaultMatPathsJsonName = "defaultMatPaths";

bool readMeshAssetInfo(Asset const& asset, MeshAssetInfo& outMeshAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMeshAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMeshAssetInfo.compressionMode = json[compressionModeJsonName];
    outMeshAssetInfo.vertexCount = json[vertexCountJsonName];
    outMeshAssetInfo.vertexBufferSize = json[vertexBufferSizeJsonName];
    outMeshAssetInfo.indexCount = json[indexCountJsonName];

    for (auto const& indBuffSizeJson : json[indexBufferSizesJsonName]) {
      outMeshAssetInfo.indexBufferSizes.push_back(
          indBuffSizeJson.get<std::size_t>());
    }

    for (auto const& matPathJson : json[defaultMatPathsJsonName]) {
      outMeshAssetInfo.defaultMatRelativePaths.push_back(
          matPathJson.get<std::string>());
    }

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
    json[hasNormalsJsonName] = meshAssetInfo.hasNormals;
    json[hasColorsJsonName] = meshAssetInfo.hasColors;
    json[hasUVJsonName] = meshAssetInfo.hasUV;
    json[indexCountJsonName] = meshAssetInfo.indexCount;

    nlohmann::json& indexBufferSizesJson = json[indexBufferSizesJsonName];

    for (std::size_t const indBuffSize : meshAssetInfo.indexBufferSizes) {
      indexBufferSizesJson.push_back(indBuffSize);
    }

    nlohmann::json& defaultMatPathsJson = json[defaultMatPathsJsonName];

    for (std::string const& matPath : meshAssetInfo.defaultMatRelativePaths) {
      defaultMatPathsJson.push_back(matPath);
    }

    outAsset.json = json.dump();

    if (meshAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(meshData);
    } else if (meshAssetInfo.compressionMode == CompressionMode::LZ4) {
      assert(meshAssetInfo.unpackedSize == meshData.size());
      return compress(meshData, outAsset.binaryBlob);
    } else {
      OBS_LOG_ERR("Unknown compression mode.");
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
