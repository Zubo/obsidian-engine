#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/serialization/serialization.hpp>

#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>
#include <variant>

namespace obsidian::asset {

constexpr char const* materialTypeJsonName = "materialType";
constexpr char const* unlitMaterialSubtypeDataJsonName = "unlitData";
constexpr char const* litMaterialSubtypeDataJsonName = "litData";
constexpr char const* pbrMaterialSubtypeDataJsonName = "pbrData";
constexpr char const* shaderJsonName = "shader";
constexpr char const* colorJsonName = "color";
constexpr char const* colorTextureJsonName = "colorTex";
constexpr char const* diffuseTextureJsonName = "diffuseTex";
constexpr char const* normalMapTextureJsonName = "normalMapTex";
constexpr char const* albedoTextureJsonName = "albedoTex";
constexpr char const* metalnessTextureJsonName = "metalnessTex";
constexpr char const* roughnessTextureJsonName = "roughnessTex";
constexpr char const* shininessJsonName = "shininess";
constexpr char const* ambientColorJsonName = "ambientColor";
constexpr char const* diffuseColorJsonName = "diffuseColor";
constexpr char const* specularColorJsonName = "specularColor";
constexpr char const* transparentJsonName = "transparent";
constexpr char const* reflectionJsonName = "reflection";
constexpr char const* hasTimerJsonName = "hasTimer";

bool writeUnlitMaterialAssetData(UnlitMaterialAssetData const& litData,
                                 nlohmann::json& outJson) {
  try {
    outJson[colorJsonName] = serialization::vecToArray(litData.color);
    outJson[colorTextureJsonName] = litData.colorTexturePath;
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool readUnlitMaterialAssetData(nlohmann::json const& json,
                                UnlitMaterialAssetData& outMaterialAssetData) {
  try {
    serialization::arrayToVector(json[colorJsonName],
                                 outMaterialAssetData.color);
    outMaterialAssetData.colorTexturePath = json[colorTextureJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool writeLitMaterialAssetData(LitMaterialAssetData const& litData,
                               nlohmann::json& outJson) {
  try {
    outJson[diffuseTextureJsonName] = litData.diffuseTexturePath;
    outJson[normalMapTextureJsonName] = litData.normalMapTexturePath;
    outJson[shininessJsonName] = litData.shininess;
    outJson[ambientColorJsonName] =
        serialization::vecToArray(litData.ambientColor);
    outJson[diffuseColorJsonName] =
        serialization::vecToArray(litData.diffuseColor);
    outJson[specularColorJsonName] =
        serialization::vecToArray(litData.specularColor);
    outJson[reflectionJsonName] = litData.reflection;
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool readLitMaterialAssetData(nlohmann::json const& json,
                              LitMaterialAssetData& outMaterialAssetData) {
  try {
    outMaterialAssetData.diffuseTexturePath = json[diffuseTextureJsonName];
    outMaterialAssetData.normalMapTexturePath = json[normalMapTextureJsonName];
    outMaterialAssetData.shininess = json[shininessJsonName];
    serialization::arrayToVector(json[ambientColorJsonName],
                                 outMaterialAssetData.ambientColor);
    serialization::arrayToVector(json[diffuseColorJsonName],
                                 outMaterialAssetData.diffuseColor);
    serialization::arrayToVector(json[specularColorJsonName],
                                 outMaterialAssetData.specularColor);
    outMaterialAssetData.reflection = json[reflectionJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool writePbrMaterialAssetData(PBRMaterialAssetData const& pbrData,
                               nlohmann::json& outJson) {
  try {
    outJson[albedoTextureJsonName] = pbrData.albedoTexturePath;
    outJson[normalMapTextureJsonName] = pbrData.normalMapTexturePath;
    outJson[metalnessTextureJsonName] = pbrData.metalnessTexturePath;
    outJson[roughnessTextureJsonName] = pbrData.roughnessTexturePath;
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool readPbrMaterialAssetData(nlohmann::json const& json,
                              PBRMaterialAssetData& outMaterialAssetData) {
  try {
    outMaterialAssetData.albedoTexturePath = json[albedoTextureJsonName];
    outMaterialAssetData.normalMapTexturePath = json[normalMapTextureJsonName];
    outMaterialAssetData.metalnessTexturePath = json[metalnessTextureJsonName];
    outMaterialAssetData.roughnessTexturePath = json[roughnessTextureJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

MaterialSubtypeData createSubtypeData(core::MaterialType matType) {
  switch (matType) {
  case core::MaterialType::unlit:
    return UnlitMaterialAssetData{};
  case core::MaterialType::lit:
    return LitMaterialAssetData{};
  case core::MaterialType::pbr:
    return PBRMaterialAssetData{};
  default:
    OBS_LOG_ERR("Invalid material type with value " +
                std::to_string((int)matType));
    return {};
  }
}

bool readMaterialAssetInfo(AssetMetadata const& assetmetadata,
                           MaterialAssetInfo& outMaterialAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(assetmetadata.json);
    outMaterialAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outMaterialAssetInfo.compressionMode = json[compressionModeJsonName];
    outMaterialAssetInfo.materialType = json[materialTypeJsonName];
    outMaterialAssetInfo.shaderPath = json[shaderJsonName];

    bool subtypeDataReadSuccess;
    switch (outMaterialAssetInfo.materialType) {
    case core::MaterialType::unlit: {
      nlohmann::json const& unlitDataJson =
          json[unlitMaterialSubtypeDataJsonName];
      subtypeDataReadSuccess = readUnlitMaterialAssetData(
          unlitDataJson, outMaterialAssetInfo.materialSubtypeData
                             .emplace<UnlitMaterialAssetData>());
      break;
    }
    case core::MaterialType::lit: {
      nlohmann::json const& litDataJson = json[litMaterialSubtypeDataJsonName];
      subtypeDataReadSuccess = readLitMaterialAssetData(
          litDataJson, outMaterialAssetInfo.materialSubtypeData
                           .emplace<LitMaterialAssetData>());
      break;
    }
    case core::MaterialType::pbr:
      nlohmann::json const& pbrDataJson = json[pbrMaterialSubtypeDataJsonName];
      subtypeDataReadSuccess = readPbrMaterialAssetData(
          pbrDataJson, outMaterialAssetInfo.materialSubtypeData
                           .emplace<PBRMaterialAssetData>());

      break;
    }

    if (!subtypeDataReadSuccess) {
      return false;
    }

    outMaterialAssetInfo.transparent = json[transparentJsonName];
    outMaterialAssetInfo.hasTimer = json[hasTimerJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset) {
  ZoneScoped;

  outAsset.metadata->type[0] = 'm';
  outAsset.metadata->type[1] = 'a';
  outAsset.metadata->type[2] = 't';
  outAsset.metadata->type[3] = 'l';

  outAsset.metadata->version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = materialAssetInfo.unpackedSize;
    json[compressionModeJsonName] = materialAssetInfo.compressionMode;
    json[materialTypeJsonName] = materialAssetInfo.materialType;
    json[shaderJsonName] = materialAssetInfo.shaderPath;

    bool subtypeDataWriteSuccess;

    switch (materialAssetInfo.materialType) {
    case obsidian::core::MaterialType::unlit: {
      subtypeDataWriteSuccess = writeUnlitMaterialAssetData(
          std::get<UnlitMaterialAssetData>(
              materialAssetInfo.materialSubtypeData),
          json[unlitMaterialSubtypeDataJsonName]);
      break;
    }
    case obsidian::core::MaterialType::lit: {
      subtypeDataWriteSuccess = writeLitMaterialAssetData(
          std::get<LitMaterialAssetData>(materialAssetInfo.materialSubtypeData),
          json[litMaterialSubtypeDataJsonName]);
      break;
    }
    case obsidian::core::MaterialType::pbr: {
      subtypeDataWriteSuccess = writePbrMaterialAssetData(
          std::get<PBRMaterialAssetData>(materialAssetInfo.materialSubtypeData),
          json[pbrMaterialSubtypeDataJsonName]);
      break;
    }
    }

    if (!subtypeDataWriteSuccess) {
      return false;
    }

    json[transparentJsonName] = materialAssetInfo.transparent;
    json[hasTimerJsonName] = materialAssetInfo.hasTimer;

    outAsset.metadata->json = json.dump();

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
