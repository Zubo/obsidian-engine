#include <obsidian/core/logging.hpp>
#include <obsidian/project/project.hpp>

#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char const** argv) {
  if (!fs::exists(SAMPLE_ASSETS_DIR)) {
    OBS_LOG_ERR("Sample asset directory missing at path " SAMPLE_ASSETS_DIR);
    return 1;
  }

  if (argc < 2) {
    OBS_LOG_ERR("Missing output project directory path parameter.");
    return 1;
  }

  obsidian::project::Project project;

  if (!project.open(argv[1])) {
    OBS_LOG_ERR("Failed to open project at path " + std::string(argv[1]));
    return 1;
  }

  return 0;
}
