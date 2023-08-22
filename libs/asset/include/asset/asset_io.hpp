#pragma once

#include <asset/asset.hpp>

#include <filesystem>
#include <string>

namespace obsidian::asset {

// returns error string
std::string loadFromFile(std::filesystem::path const& path, Asset& outAsset);
// returns error string
std::string saveToFile(std::filesystem::path const& path, Asset const& asset);

} /*namespace obsidian::asset*/
