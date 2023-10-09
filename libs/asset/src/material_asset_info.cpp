#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <nlohmann/json.hpp>

#include <cassert>

namespace obsidian::asset {

constexpr char const* materialTypeJsonName = "materialType";
constexpr char const* shaderJsonName = "shader";
constexpr char const* albedoTextureJsonName = "albedoTex";

bool readMaterialAssetInfo(Asset const& asset,
                           MaterialAssetInfo& outMaterialAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMaterialAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMaterialAssetInfo.compressionMode = json[compressionModeJsonName];
    outMaterialAssetInfo.materialType = json[materialTypeJsonName];
    outMaterialAssetInfo.shaderPath = json[shaderJsonName];
    outMaterialAssetInfo.albedoTexturePath = json[albedoTextureJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset) {
  outAsset.type[0] = 'm';
  outAsset.type[1] = 'a';
  outAsset.type[2] = 't';
  outAsset.type[3] = 'l';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = materialAssetInfo.unpackedSize;
    json[compressionModeJsonName] = materialAssetInfo.compressionMode;
    json[materialTypeJsonName] = materialAssetInfo.materialType;
    json[shaderJsonName] = materialAssetInfo.shaderPath;
    json[albedoTextureJsonName] = materialAssetInfo.albedoTexturePath;

    outAsset.json = json.dump();

    if (materialAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(materialData);
    } else if (materialAssetInfo.compressionMode == CompressionMode::LZ4) {
      assert(materialAssetInfo.unpackedSize == materialData.size());
      return compress(materialData, outAsset.binaryBlob);
    } else {
      OBS_LOG_ERR("Error: Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
