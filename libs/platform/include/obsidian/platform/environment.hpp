#pragma once

#include <filesystem>

namespace obsidian::platform {

std::filesystem::path getExecutableFilePath();

std::filesystem::path getExecutableDirectoryPath();

} /*namespace obsidian::platform*/
