#pragma once

#include <obsidian/asset/texture_asset_info.hpp>
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
  AssetConverter(task::TaskExecutor& taskExecutor);

  bool convertAsset(std::filesystem::path const& srcPath,
                    std::filesystem::path const& dstPath);

private:
  std::optional<asset::TextureAssetInfo> convertImgToAsset(
      std::filesystem::path const& srcPath,
      std::filesystem::path const& dstPath,
      std::optional<core::TextureFormat> overrideTextureFormat = std::nullopt);

  bool convertObjToAsset(std::filesystem::path const& srcPath,
                         std::filesystem::path const& dstPath);

  bool convertGltfToAsset(std::filesystem::path const& srcPath,
                          std::filesystem::path const& dstPath);

  bool convertSpirvToAsset(std::filesystem::path const& srcPath,
                           std::filesystem::path const& dstPath);

  template <typename MaterialType>
  std::vector<std::string>
  extractMaterials(std::filesystem::path const& srcDirPath,
                   std::filesystem::path const& projectPath,
                   std::vector<MaterialType> const& materials,
                   VertexContentInfo const& info);

  std::optional<asset::TextureAssetInfo> getOrCreateTexture(
      std::filesystem::path const& srcPath,
      std::filesystem::path const& dstPath,
      std::optional<core::TextureFormat> overrideTextureFormat = std::nullopt);

  task::TaskExecutor& _taskExecutor;
};

} /*namespace obsidian::asset_converter*/
