#pragma once

#include <asset/asset.hpp>

#include <filesystem>

namespace obsidian::asset {

bool loadFromFile(std::filesystem::path const& path, Asset& outAsset);

bool saveToFile(std::filesystem::path const& path, Asset const& asset);

} /*namespace obsidian::asset*/
