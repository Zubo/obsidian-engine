#pragma once

#include <obsidian/asset/asset.hpp>

#include <filesystem>

namespace obsidian::asset {

bool loadAssetFromFile(std::filesystem::path const& path, Asset& outAsset);

bool saveToFile(std::filesystem::path const& path, Asset const& asset);

} /*namespace obsidian::asset*/
