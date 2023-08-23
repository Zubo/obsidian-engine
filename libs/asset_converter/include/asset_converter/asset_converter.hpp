#pragma once

#include <filesystem>

namespace obsidian::asset_converter {

bool convertAsset(std::filesystem::path const& srcPath,
                  std::filesystem::path const& dstPath);

} /*namespace obsidian::asset_converter*/
