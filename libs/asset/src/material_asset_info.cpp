#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <nlohmann/json.hpp>

#include <cassert>

namespace obsidian::asset {

constexpr char const* materialTypeJsonName = "materialType";
constexpr char const* shaderJsonName = "shader";
constexpr char const* diffuseTextureJsonName = "diffuseTex";
constexpr char const* normalMapTextureJsonName = "normalMapTex";
constexpr char const* shininessJsonName = "shininess";
constexpr char const* ambientColorJsonName = "ambientColor";
constexpr char const* diffuseColorJsonName = "diffuseColor";
constexpr char const* specularColorJsonName = "specularColor";
constexpr char const* transparentJsonName = "transparent";

bool readMaterialAssetInfo(Asset const& asset,
                           MaterialAssetInfo& outMaterialAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outMaterialAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMaterialAssetInfo.compressionMode = json[compressionModeJsonName];
    outMaterialAssetInfo.materialType = json[materialTypeJsonName];
    outMaterialAssetInfo.shaderPath = json[shaderJsonName];
    outMaterialAssetInfo.diffuseTexturePath = json[diffuseTextureJsonName];
    outMaterialAssetInfo.normalMapTexturePath = json[normalMapTextureJsonName];
    outMaterialAssetInfo.shininess = json[shininessJsonName];
    outMaterialAssetInfo.ambientColor.r = json[ambientColorJsonName]["r"];
    outMaterialAssetInfo.ambientColor.g = json[ambientColorJsonName]["g"];
    outMaterialAssetInfo.ambientColor.b = json[ambientColorJsonName]["b"];
    outMaterialAssetInfo.ambientColor.a = json[ambientColorJsonName]["a"];
    outMaterialAssetInfo.diffuseColor.r = json[diffuseColorJsonName]["r"];
    outMaterialAssetInfo.diffuseColor.g = json[diffuseColorJsonName]["g"];
    outMaterialAssetInfo.diffuseColor.b = json[diffuseColorJsonName]["b"];
    outMaterialAssetInfo.diffuseColor.a = json[diffuseColorJsonName]["a"];
    outMaterialAssetInfo.specularColor.r = json[specularColorJsonName]["r"];
    outMaterialAssetInfo.specularColor.g = json[specularColorJsonName]["g"];
    outMaterialAssetInfo.specularColor.b = json[specularColorJsonName]["b"];
    outMaterialAssetInfo.specularColor.a = json[specularColorJsonName]["a"];
    outMaterialAssetInfo.transparent = json[transparentJsonName];
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
    json[diffuseTextureJsonName] = materialAssetInfo.diffuseTexturePath;
    json[normalMapTextureJsonName] = materialAssetInfo.normalMapTexturePath;
    json[shininessJsonName] = materialAssetInfo.shininess;
    json[ambientColorJsonName]["r"] = materialAssetInfo.ambientColor.r;
    json[ambientColorJsonName]["g"] = materialAssetInfo.ambientColor.g;
    json[ambientColorJsonName]["b"] = materialAssetInfo.ambientColor.b;
    json[ambientColorJsonName]["a"] = materialAssetInfo.ambientColor.a;
    json[diffuseColorJsonName]["r"] = materialAssetInfo.diffuseColor.r;
    json[diffuseColorJsonName]["g"] = materialAssetInfo.diffuseColor.g;
    json[diffuseColorJsonName]["b"] = materialAssetInfo.diffuseColor.b;
    json[diffuseColorJsonName]["a"] = materialAssetInfo.diffuseColor.a;
    json[specularColorJsonName]["r"] = materialAssetInfo.specularColor.r;
    json[specularColorJsonName]["g"] = materialAssetInfo.specularColor.g;
    json[specularColorJsonName]["b"] = materialAssetInfo.specularColor.b;
    json[specularColorJsonName]["a"] = materialAssetInfo.specularColor.a;
    json[transparentJsonName] = materialAssetInfo.transparent;

    outAsset.json = json.dump();

    if (materialAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(materialData);
    } else if (materialAssetInfo.compressionMode == CompressionMode::LZ4) {
      assert(materialAssetInfo.unpackedSize == materialData.size());
      return compress(materialData, outAsset.binaryBlob);
    } else {
      OBS_LOG_ERR("Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
