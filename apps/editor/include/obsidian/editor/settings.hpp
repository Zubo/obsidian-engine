#pragma once

#include <filesystem>

namespace obsidian::editor {

std::filesystem::path getLastOpenProject();

void setLastOpenProject(std::filesystem::path const& path);

} /*namespace obsidian::editor*/
