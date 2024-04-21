#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <filesystem>
#include <thread>

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

  unsigned int nCores = std::max(std::thread::hardware_concurrency(), 2u);

  obsidian::task::TaskExecutor taskExecutor;
  taskExecutor.initAndRun({{obsidian::task::TaskType::general, nCores}});

  obsidian::asset_converter::AssetConverter converter{taskExecutor};

  fs::path const modelPath =
      fs::path{SAMPLE_ASSETS_DIR} / "SM_Deccer_Cubes_Textured.glb";

  if (!converter.convertAsset(modelPath, argv[1] / modelPath.stem())) {
    OBS_LOG_ERR("Failed to convert " + modelPath.string());
    return 1;
  }

  return 0;
}
