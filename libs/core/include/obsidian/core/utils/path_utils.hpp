#pragma once

#include <filesystem>
#include <string>

namespace obsidian::core::utils {

// Recursively searches the directory tree from startDir to the root dir for a
// directory with name dirName and returns its path
std::filesystem::path findDirInParentTree(std::filesystem::path const& startDir,
                                          std::string const& dirName);

} /*namespace obsidian::core::utils*/
