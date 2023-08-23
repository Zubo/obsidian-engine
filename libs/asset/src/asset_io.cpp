#include <asset/asset.hpp>
#include <asset/asset_io.hpp>
#include <core/logging.hpp>

#include <fstream>
#include <ios>

namespace fs = std::filesystem;

namespace obsidian::asset {

bool loadFromFile(fs::path const& path, Asset& outAsset) {
  std::ifstream inputFileStream;
  inputFileStream.exceptions(std::ios::failbit);

  try {
    inputFileStream.open(path, std::ios_base::in | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  if (!inputFileStream) {
    OBS_LOG_ERR("Error: Failed to load file: " + path.string());
    return false;
  }

  inputFileStream.read(outAsset.type, std::size(outAsset.type));
  inputFileStream.read(reinterpret_cast<char*>(&outAsset.version),
                       sizeof(outAsset.version));

  Asset::SizeType jsonSize;
  inputFileStream.read(reinterpret_cast<char*>(&jsonSize), sizeof(jsonSize));

  Asset::SizeType binaryBlobSize;
  inputFileStream.read(reinterpret_cast<char*>(&binaryBlobSize),
                       sizeof(binaryBlobSize));

  outAsset.json.resize(jsonSize);
  inputFileStream.read(outAsset.json.data(), outAsset.json.length());

  outAsset.binaryBlob.resize(binaryBlobSize);
  inputFileStream.read(outAsset.binaryBlob.data(), outAsset.binaryBlob.size());

  return true;
}

bool saveToFile(fs::path const& path, Asset const& asset) {
  std::ofstream outputFileStream;
  outputFileStream.exceptions(std::ios_base::failbit);

  try {
    outputFileStream.open(path, std::ios_base::out | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  outputFileStream.write(asset.type, std::size(asset.type));
  outputFileStream.write(reinterpret_cast<char const*>(&asset.version),
                         sizeof(asset.version));

  Asset::SizeType const jsonSize{asset.json.size()};
  outputFileStream.write(reinterpret_cast<char const*>(&jsonSize),
                         sizeof(jsonSize));

  Asset::SizeType const binaryBlobSize{asset.binaryBlob.size()};
  outputFileStream.write(reinterpret_cast<char const*>(&binaryBlobSize),
                         sizeof(binaryBlobSize));

  outputFileStream.write(asset.json.data(), asset.json.size());
  outputFileStream.write(asset.binaryBlob.data(), asset.binaryBlob.size());

  return true;
}

} /*namespace obsidian::asset*/
