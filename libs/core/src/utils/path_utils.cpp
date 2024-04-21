#include <obsidian/core/utils/path_utils.hpp>

namespace fs = std::filesystem;

fs::path
obsidian::core::utils::findDirInParentTree(fs::path const& startDir,
                                           std::string const& dirName) {
  fs::path currentPath = startDir;

  while (!currentPath.empty()) {

    fs::path targetPath = currentPath / dirName;

    if (fs::exists(targetPath) && fs::is_directory(targetPath)) {
      return targetPath;
    }

    currentPath =
        currentPath.has_parent_path() ? currentPath.parent_path() : "";
  }

  return {};
}
