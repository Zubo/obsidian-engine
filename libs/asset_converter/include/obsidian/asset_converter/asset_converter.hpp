#pragma once

#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/vertex_content_info.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tinyobj {

struct material_t;

} /*namespace tinyobj*/

namespace obsidian::task {

class TaskExecutor;

} /*namespace obsidian::task */

namespace obsidian::asset_converter {

extern std::unordered_map<std::string, std::string> extensionMap;

struct VertexContentInfo;

class AssetConverter {
public:
  using TextureAssetInfoMap =
      std::unordered_map<std::string, std::optional<asset::TextureAssetInfo>>;

  AssetConverter(task::TaskExecutor& taskExecutor);

  bool convertAsset(std::filesystem::path const& srcPath,
                    std::filesystem::path const& dstPath);

  void setMaterialType(core::MaterialType matType);

private:
  std::optional<asset::TextureAssetInfo> convertImgToAsset(
      std::filesystem::path const& srcPath,
      std::filesystem::path const& dstPath, bool generateMips,
      std::optional<core::TextureFormat> overrideTextureFormat = std::nullopt);

  bool convertObjToAsset(std::filesystem::path const& srcPath,
                         std::filesystem::path const& dstPath);

  bool convertGltfToAsset(std::filesystem::path const& srcPath,
                          std::filesystem::path const& dstPath);

  bool convertSpirvToAsset(std::filesystem::path const& srcPath,
                           std::filesystem::path const& dstPath);

  template <typename MaterialType>
  TextureAssetInfoMap
  extractTexturesForMaterials(std::filesystem::path const& srcDirPath,
                              std::filesystem::path const& projectPath,
                              std::vector<MaterialType> const& materials,
                              bool tryFindingTextureSubdir);

  using MaterialPathTable =
      std::array<std::vector<std::string>, intRepresentationMax() + 1>;

  template <typename MaterialType>
  MaterialPathTable
  extractMaterials(std::filesystem::path const& srcDirPath,
                   std::filesystem::path const& projectPath,
                   TextureAssetInfoMap const& textureAssetInfoMap,
                   std::vector<MaterialType> const& materials,
                   std::size_t totalMaterialCount);

  std::optional<asset::TextureAssetInfo> getOrImportTexture(
      std::filesystem::path const& srcPath,
      std::filesystem::path const& dstPath,
      std::optional<core::TextureFormat> overrideTextureFormat = std::nullopt);

  task::TaskExecutor& _taskExecutor;
  core::MaterialType _materialType = core::MaterialType::unlit;
};

} /*namespace obsidian::asset_converter*/
