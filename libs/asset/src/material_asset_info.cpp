#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

namespace obsidian::asset {

constexpr char const* materialTypeJsonName = "materialType";
constexpr char const* vertexShaderJsonName = "vertexShader";
constexpr char const* fragmentShaderJsonName = "fragmentShader";
constexpr char const* albedoTextureJsonName = "albedoTex";

bool readMaterialAssetInfo(Asset const& asset,
                           MaterialAssetInfo& outMaterialAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMaterialAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMaterialAssetInfo.compressionMode = json[compressionModeJsonName];
    outMaterialAssetInfo.materialType = json[materialTypeJsonName];
    outMaterialAssetInfo.vertexShaderPath = json[vertexShaderJsonName];
    outMaterialAssetInfo.fragmentShaderPath = json[fragmentShaderJsonName];
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
    json[vertexShaderJsonName] = materialAssetInfo.vertexShaderPath;
    json[fragmentShaderJsonName] = materialAssetInfo.fragmentShaderPath;
    json[albedoTextureJsonName] = materialAssetInfo.albedoTexturePath;

    outAsset.json = json.dump();

    if (materialAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(materialData);
    } else if (materialAssetInfo.compressionMode == CompressionMode::LZ4) {
      std::size_t compressedSize =
          LZ4_compressBound(materialAssetInfo.unpackedSize);

      outAsset.binaryBlob.resize(compressedSize);

      LZ4_compress_default(reinterpret_cast<char const*>(materialData.data()),
                           outAsset.binaryBlob.data(),
                           materialAssetInfo.unpackedSize, compressedSize);
    } else {
      OBS_LOG_ERR("Error: Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} /*namespace obsidian::asset*/
