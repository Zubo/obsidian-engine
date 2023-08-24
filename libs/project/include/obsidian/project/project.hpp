#pragma once

#include <filesystem>

namespace obsidian::project {

class Project {
public:
  bool open(std::filesystem::path projectRootPath);

  std::filesystem::path getOpenProjectPath() const;

  std::filesystem::path
  getAbsolutePath(std::filesystem::path const& relativePath) const;

  std::filesystem::path
  getRelativeToProjectRootPath(std::filesystem::path const& absolutePath) const;

private:
  std::filesystem::path _projectRootPath;
};

} /*namespace obsidian::project*/
