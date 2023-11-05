#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/core/logging.hpp>

#include <tracy/Tracy.hpp>

#include <fstream>
#include <ios>

namespace fs = std::filesystem;

namespace obsidian::asset {

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
    outAsset.metadata = {};

    inputFileStream.read(outAsset.metadata->type,
                         std::size(outAsset.metadata->type));
    inputFileStream.read(reinterpret_cast<char*>(&outAsset.metadata->version),
                         sizeof(outAsset.metadata->version));

    inputFileStream.read(reinterpret_cast<char*>(&outAsset.metadata->jsonSize),
                         sizeof(AssetMetadata::SizeType));

    inputFileStream.read(
        reinterpret_cast<char*>(&outAsset.metadata->binaryBlobSize),
        sizeof(AssetMetadata::SizeType));

    outAsset.metadata->json.resize(outAsset.metadata->jsonSize);
    inputFileStream.read(outAsset.metadata->json.data(),
                         outAsset.metadata->json.length());
  } else {
    std::size_t metadataSize =
        sizeof(AssetMetadata::type) + sizeof(AssetMetadata::version) +
        sizeof(AssetMetadata::jsonSize) +
        sizeof(AssetMetadata::binaryBlobSize) + outAsset.metadata->jsonSize;
    inputFileStream.seekg(metadataSize);
  }

  outAsset.binaryBlob.resize(outAsset.metadata->binaryBlobSize);
  inputFileStream.read(outAsset.binaryBlob.data(), outAsset.binaryBlob.size());

  return true;
}

bool saveToFile(fs::path const& path, Asset const& asset) {
  ZoneScoped;

  std::ofstream outputFileStream;
  outputFileStream.exceptions(std::ios_base::failbit);

  try {
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
