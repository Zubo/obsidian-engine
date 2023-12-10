#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/core/logging.hpp>

#include <tracy/Tracy.hpp>

#include <filesystem>
#include <fstream>
#include <ios>

namespace fs = std::filesystem;

namespace obsidian::asset {

void readAssetMetadata(std::ifstream& inputFileStream,
                       asset::AssetMetadata& outAssetMetadata) {
  inputFileStream.read(outAssetMetadata.type, std::size(outAssetMetadata.type));
  inputFileStream.read(reinterpret_cast<char*>(&outAssetMetadata.version),
                       sizeof(outAssetMetadata.version));

  inputFileStream.read(reinterpret_cast<char*>(&outAssetMetadata.jsonSize),
                       sizeof(AssetMetadata::SizeType));

  inputFileStream.read(
      reinterpret_cast<char*>(&outAssetMetadata.binaryBlobSize),
      sizeof(AssetMetadata::SizeType));

  outAssetMetadata.json.resize(outAssetMetadata.jsonSize);
  inputFileStream.read(outAssetMetadata.json.data(),
                       outAssetMetadata.json.length());
}

bool loadAssetMetadataFromFile(std::filesystem::path const& path,
                               asset::AssetMetadata& outAssetMetadata) {
  std::ifstream inputFileStream;
  inputFileStream.exceptions(std::ios::failbit);

  try {
    inputFileStream.open(path, std::ios_base::in | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  readAssetMetadata(inputFileStream, outAssetMetadata);

  return true;
}

bool loadAssetFromFile(fs::path const& path, Asset& outAsset) {
  ZoneScoped;

  std::ifstream inputFileStream;
  inputFileStream.exceptions(std::ios::failbit);

  try {
    inputFileStream.open(path, std::ios_base::in | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  if (!inputFileStream) {
    OBS_LOG_ERR("Failed to load file: " + path.string());
    return false;
  }

  bool const metadataLoaded = outAsset.metadata.has_value();

  if (!metadataLoaded) {
    outAsset.metadata.emplace();
    readAssetMetadata(inputFileStream, *outAsset.metadata);
  } else {
    std::size_t metadataSize =
        sizeof(AssetMetadata::type) + sizeof(AssetMetadata::jsonSize) +
        sizeof(AssetMetadata::binaryBlobSize) + sizeof(AssetMetadata::version) +
        outAsset.metadata->jsonSize;
    inputFileStream.seekg(metadataSize);
  }

  outAsset.binaryBlob.resize(outAsset.metadata->binaryBlobSize);
  inputFileStream.read(outAsset.binaryBlob.data(), outAsset.binaryBlob.size());

  outAsset.isLoaded = true;

  return true;
}

bool saveToFile(fs::path const& path, Asset const& asset) {
  ZoneScoped;

  std::ofstream outputFileStream;
  outputFileStream.exceptions(std::ios_base::failbit);

  try {
    fs::path const directoryPath = path.parent_path();
    if (!fs::exists(directoryPath)) {
      fs::create_directories(directoryPath);
    }
    outputFileStream.open(path, std::ios_base::out | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  outputFileStream.write(asset.metadata->type, std::size(asset.metadata->type));
  outputFileStream.write(
      reinterpret_cast<char const*>(&asset.metadata->version),
      sizeof(asset.metadata->version));

  AssetMetadata::SizeType const jsonSize{asset.metadata->json.size()};
  outputFileStream.write(reinterpret_cast<char const*>(&jsonSize),
                         sizeof(jsonSize));

  AssetMetadata::SizeType const binaryBlobSize{asset.binaryBlob.size()};
  outputFileStream.write(reinterpret_cast<char const*>(&binaryBlobSize),
                         sizeof(binaryBlobSize));

  outputFileStream.write(asset.metadata->json.data(),
                         asset.metadata->json.size());
  outputFileStream.write(asset.binaryBlob.data(), asset.binaryBlob.size());

  return true;
}

} /*namespace obsidian::asset*/
