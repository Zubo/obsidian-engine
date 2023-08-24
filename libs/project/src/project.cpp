#include <obsidian/core/logging.hpp>
#include <obsidian/project/project.hpp>

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
