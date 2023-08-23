#include <core/logging.hpp>
#include <project/project.hpp>

#include <filesystem>

using namespace obsidian::project;

namespace fs = std::filesystem;

bool Project::open(fs::path projectRootPath) {
  if (fs::exists(projectRootPath) && !fs::is_directory(projectRootPath)) {
    OBS_LOG_ERR("Project path " + projectRootPath.string() +
                " is not a directory.");
    return false;
  }

  if (!fs::exists(projectRootPath)) {
    fs::create_directory(projectRootPath);
  }

  _projectRootPath = std::move(projectRootPath);

  return true;
}

fs::path Project::getAbsolutePath(fs::path const& relativePath) const {
  return _projectRootPath / relativePath;
}

fs::path
Project::getRelativeToProjectRootPath(fs::path const& absolutePath) const {
  return fs::relative(absolutePath, _projectRootPath);
}
