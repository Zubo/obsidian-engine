#include <obsidian/core/logging.hpp>
#include <obsidian/platform/environment.hpp>
#include <obsidian/project/project.hpp>

#include <exception>
#include <filesystem>

using namespace obsidian;
using namespace obsidian::project;

namespace fs = std::filesystem;

fs::path getInitialAssetsPath() {
  fs::path currentPath = platform::getExecutableDirectoryPath();

  while (!currentPath.empty() && currentPath.has_parent_path()) {
    fs::path standardAssetsPath = currentPath / "standard-assets";

    if (fs::exists(standardAssetsPath) &&
        fs::is_directory(standardAssetsPath)) {
      return standardAssetsPath;
    }

    currentPath = currentPath.parent_path();
  }

  return {};
}

bool importStandardAssetsToProject(fs::path const& standardAssetsPath,
                                   fs::path const& projectPath) {
  try {
    fs::copy(standardAssetsPath, projectPath,
             fs::copy_options::recursive |
                 fs ::copy_options::overwrite_existing);
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool Project::open(fs::path projectRootPath) {
  if (fs::exists(projectRootPath) && !fs::is_directory(projectRootPath)) {
    OBS_LOG_ERR("Project path " + projectRootPath.string() +
                " is not a directory.");
    return false;
  }

  if (!fs::exists(projectRootPath)) {
    fs::create_directory(projectRootPath);
  }

  fs::path const standardAssetsPath = getInitialAssetsPath();

  if (standardAssetsPath.empty()) {
    OBS_LOG_ERR("The initial assets are missing on path " +
                standardAssetsPath.string());
    return false;
  }

  if (!importStandardAssetsToProject(standardAssetsPath, projectRootPath)) {
    return false;
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

std::filesystem::path Project::getOpenProjectPath() const {
  return _projectRootPath;
}

std::vector<fs::path>
Project::getAllFilesWithExtension(std::string_view extension) const {
  if (_projectRootPath.empty()) {
    OBS_LOG_WARN("Project not open.");
    return {};
  }
  std::vector<fs::path> result;

  for (fs::directory_entry const& p :
       fs::recursive_directory_iterator(_projectRootPath)) {
    if (!p.is_regular_file()) {
      continue;
    }

    if (p.path().extension() == extension) {
      result.push_back(getRelativeToProjectRootPath(p.path()));
    }
  }

  return result;
}
