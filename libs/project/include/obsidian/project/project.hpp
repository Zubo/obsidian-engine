#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace obsidian::project {

class Project {
public:
  bool open(std::filesystem::path projectRootPath);

  std::filesystem::path getOpenProjectPath() const;

  std::filesystem::path
  getAbsolutePath(std::filesystem::path const& relativePath) const;

  std::filesystem::path
  getRelativeToProjectRootPath(std::filesystem::path const& absolutePath) const;

  std::vector<std::filesystem::path>
  getAllFilesWithExtension(std::string_view extension) const;

private:
  std::filesystem::path _projectRootPath;
};

} /*namespace obsidian::project*/
