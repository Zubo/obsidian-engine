#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

void reportInvalidArguments() {
  OBS_LOG_ERR("Invalid command line arguments. The required command line "
              "arguments are:\n"
              "-s <source-dir-path>\n"
              "-d <destination-dir-path>\n");
}

int main(int argc, char const** argv) {
  if (argc != 5) {
    reportInvalidArguments();
    return -1;
  }

  std::optional<fs::path> srcPath;
  std::optional<fs::path> dstPath;

  for (std::size_t i = 1; i < argc - 1; ++i) {
    if (std::strcmp(argv[i], "-s") == 0) {
      srcPath = argv[i + 1];
      ++i;
    } else if (std::strcmp(argv[i], "-d") == 0) {
      dstPath = argv[i + 1];
      ++i;
    }
  }

  if (!srcPath || !dstPath) {
    reportInvalidArguments();
    return -1;
  }

  if (!fs::exists(*srcPath)) {
    OBS_LOG_ERR("The src path " + srcPath->string() + " doesn't exist.");
    return -1;
  }

  if (fs::exists(*dstPath)) {
    if (!fs::is_directory(*dstPath)) {
      OBS_LOG_ERR("The destination path is not a directory.");
      return -1;
    }
  } else {
    if (!fs::create_directories(*dstPath)) {
      OBS_LOG_ERR("Couldn't create directory at path " + dstPath->string());
      return -1;
    }
  }

  fs::directory_iterator dirIter(*srcPath);

  unsigned int nCores = std::max(std::thread::hardware_concurrency(), 2u);

  obsidian::task::TaskExecutor taskExecutor;
  taskExecutor.initAndRun({{obsidian::task::TaskType::general, nCores}});
  obsidian::asset_converter::AssetConverter converter{taskExecutor};

  for (auto const& entry : dirIter) {
    if (!std::filesystem::is_regular_file(entry)) {
      continue;
    }

    fs::path srcFilePath = entry.path();
    auto const extensionMapping = obsidian::asset_converter::extensionMap.find(
        srcFilePath.extension().string());

    if (extensionMapping == obsidian::asset_converter::extensionMap.cend()) {
      continue;
    }

    fs::path fileName = srcFilePath.filename();
    fileName.replace_extension(extensionMapping->second);
    fs::path dstFilePath = *dstPath / fileName;

    if (!converter.convertAsset(srcFilePath, dstFilePath)) {
      return -1;
    }
  }

  return 0;
}
