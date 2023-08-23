#include <project/project.hpp>

#include <filesystem>
#include <iostream>

using namespace obsidian::project;

namespace fs = std::filesystem;

bool Project::open(fs::path projectRootPath) {
  if (fs::exists(projectRootPath) && !fs::is_directory(projectRootPath)) {
    std::cout << "Error: Project path needs to be a directory" << std::endl;
    return false;
  }

  if (!fs::exists(projectRootPath)) {
    fs::create_directory(projectRootPath);
  }

  _projectRootPath = std::move(projectRootPath);
}

fs::path Project::getAbsolutePath(fs::path const& relativePath) const {
  return _projectRootPath / relativePath;
}

fs::path
Project::getRelativeToProjectRootPath(fs::path const& absolutePath) const {
  return fs::relative(absolutePath, _projectRootPath);
}
