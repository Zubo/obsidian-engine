#include <algorithm>
#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <exception>
#include <iterator>

namespace obsidian::asset {

constexpr char const* vertexCountJsonName = "vertexCount";
constexpr char const* vertexBufferSizeJsonName = "vertexBufferSize";
constexpr char const* indexBufferSizesJsonName = "indexBufferSizes";
constexpr char const* indexCountJsonName = "indexCount";
constexpr char const* hasNormalsJsonName = "hasNormals";
constexpr char const* hasColorsJsonName = "hasColors";
constexpr char const* hasUVJsonName = "hasUV";
constexpr char const* hasTangentsJsonName = "hasTangents";
constexpr char const* defaultMatPathsJsonName = "defaultMatPaths";
constexpr char const* aabbJsonName = "aabb";
constexpr char const* aabbTopRightJsonName = "topRight";
constexpr char const* aabbBottomLeftJsonName = "bottomLeft";

bool readMeshAssetInfo(AssetMetadata const& assetMetadata,
                       MeshAssetInfo& outMeshAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(assetMetadata.json);
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

    outMeshAssetInfo.aabb.topCorner.x =
        json[aabbJsonName][aabbTopRightJsonName]["x"];
    outMeshAssetInfo.aabb.topCorner.y =
        json[aabbJsonName][aabbTopRightJsonName]["y"];
    outMeshAssetInfo.aabb.topCorner.z =
        json[aabbJsonName][aabbTopRightJsonName]["z"];

    outMeshAssetInfo.aabb.bottomCorner.x =
        json[aabbJsonName][aabbBottomLeftJsonName]["x"];
    outMeshAssetInfo.aabb.bottomCorner.y =
        json[aabbJsonName][aabbBottomLeftJsonName]["y"];
    outMeshAssetInfo.aabb.bottomCorner.z =
        json[aabbJsonName][aabbBottomLeftJsonName]["z"];

    outMeshAssetInfo.hasNormals = json[hasNormalsJsonName];
    outMeshAssetInfo.hasColors = json[hasColorsJsonName];
    outMeshAssetInfo.hasUV = json[hasUVJsonName];
    outMeshAssetInfo.hasTangents = json[hasTangentsJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packMeshAsset(MeshAssetInfo const& meshAssetInfo,
                   std::vector<char> meshData, Asset& outAsset) {
  ZoneScoped;

  if (!outAsset.metadata) {
    outAsset.metadata.emplace();
  }

  outAsset.metadata->type[0] = 'm';
  outAsset.metadata->type[1] = 'e';
  outAsset.metadata->type[2] = 's';
  outAsset.metadata->type[3] = 'h';

  outAsset.metadata->version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = meshAssetInfo.unpackedSize;
    json[compressionModeJsonName] = meshAssetInfo.compressionMode;
    json[vertexCountJsonName] = meshAssetInfo.vertexCount;
    json[vertexBufferSizeJsonName] = meshAssetInfo.vertexBufferSize;

    json[aabbJsonName][aabbTopRightJsonName]["x"] =
        meshAssetInfo.aabb.topCorner.x;
    json[aabbJsonName][aabbTopRightJsonName]["y"] =
        meshAssetInfo.aabb.topCorner.y;
    json[aabbJsonName][aabbTopRightJsonName]["z"] =
        meshAssetInfo.aabb.topCorner.z;

    json[aabbJsonName][aabbBottomLeftJsonName]["x"] =
        meshAssetInfo.aabb.bottomCorner.x;
    json[aabbJsonName][aabbBottomLeftJsonName]["y"] =
        meshAssetInfo.aabb.bottomCorner.y;
    json[aabbJsonName][aabbBottomLeftJsonName]["z"] =
        meshAssetInfo.aabb.bottomCorner.z;

    json[hasNormalsJsonName] = meshAssetInfo.hasNormals;
    json[hasColorsJsonName] = meshAssetInfo.hasColors;
    json[hasUVJsonName] = meshAssetInfo.hasUV;
    json[hasTangentsJsonName] = meshAssetInfo.hasTangents;
    json[indexCountJsonName] = meshAssetInfo.indexCount;

    nlohmann::json& indexBufferSizesJson = json[indexBufferSizesJsonName];

    std::copy(meshAssetInfo.indexBufferSizes.cbegin(),
              meshAssetInfo.indexBufferSizes.cend(),
              std::back_inserter(indexBufferSizesJson));

    nlohmann::json& defaultMatPathsJson = json[defaultMatPathsJsonName];

    std::copy(meshAssetInfo.defaultMatRelativePaths.cbegin(),
              meshAssetInfo.defaultMatRelativePaths.cend(),
              std::back_inserter(defaultMatPathsJson));

    outAsset.metadata->json = json.dump();

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

} /*namespace obsidian::asset*/
