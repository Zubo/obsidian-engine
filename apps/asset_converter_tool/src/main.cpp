#include "asset_converter/asset_converter.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

void reportInvalidArguments() {
  std::cout << "Invalid command line arguments. The required command line "
               "arguments are:"
            << std::endl
            << "-s <source-dir-path>" << std::endl
            << "-d <destination-dir-path>" << std::endl;
}

int main(int argc, char** argv) {
  static std::unordered_map<std::string, std::string> extensionMap = {
      {".png", ".obstex"}, {".obj", ".obsmesh"}};

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
    std::cout << "Error: the src path " << *srcPath << " doesn't exist.";
    return -1;
  }

  if (fs::exists(*dstPath)) {
    if (!fs::is_directory(*dstPath)) {
      std::cout << "Error: the destination path is not a directory.";
      return -1;
    }
  } else {
    if (!fs::create_directory(*dstPath))
      std::cout << "Error: Couldn't create directory at path " << *dstPath
                << std::endl;
    return -1;
  }

  for (auto const& entry : fs::directory_iterator(*srcPath)) {
    if (!std::filesystem::is_regular_file(entry)) {
      continue;
    }

    fs::path srcFilePath = entry.path();
    auto const extensionMapping =
        extensionMap.find(srcFilePath.extension().string());

    if (extensionMapping == extensionMap.cend()) {
      continue;
    }

    fs::path fileName = srcFilePath.filename();
    fileName.replace_extension(extensionMapping->second);
    fs::path dstFilePath = *dstPath / fileName;

    obsidian::asset_converter::convertAsset(srcFilePath, dstFilePath);
  }

  return 0;
}
