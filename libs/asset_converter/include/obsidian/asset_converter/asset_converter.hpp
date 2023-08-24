#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace obsidian::asset_converter {

extern std::unordered_map<std::string, std::string> extensionMap;

bool convertAsset(std::filesystem::path const& srcPath,
                  std::filesystem::path const& dstPath);

} /*namespace obsidian::asset_converter*/
