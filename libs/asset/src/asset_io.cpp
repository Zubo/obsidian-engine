#include "asset/asset.hpp"
#include <asset/asset_io.hpp>

#include <fstream>
#include <ios>
#include <iostream>

namespace fs = std::filesystem;

namespace obsidian::asset {

std::string loadFromFile(fs::path const& path, Asset& outAsset) {
  std::ifstream inputFileStream;
  inputFileStream.exceptions(std::ios::failbit);

  try {
    inputFileStream.open(path, std::ios_base::in | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    return std::string(e.what());
  }

  inputFileStream.read(outAsset.type, std::size(outAsset.type));

  Asset::SizeType jsonSize;
  inputFileStream.read(reinterpret_cast<char*>(&jsonSize), sizeof(jsonSize));

  Asset::SizeType binaryBlobSize;
  inputFileStream.read(reinterpret_cast<char*>(&binaryBlobSize),
                       sizeof(binaryBlobSize));

  outAsset.json.resize(jsonSize);
  inputFileStream.read(outAsset.json.data(), outAsset.json.length());

  outAsset.binaryBlob.resize(binaryBlobSize);
  inputFileStream.read(outAsset.binaryBlob.data(), outAsset.binaryBlob.size());

  return {};
}

std::string saveToFile(fs::path const& path, Asset const& asset) {
  std::ofstream outputFileStream;
  outputFileStream.exceptions(std::ios_base::failbit);

  try {
    outputFileStream.open(path, std::ios_base::out | std::ios_base::binary);
  } catch (std::ios_base::failure const& e) {
    return std::string{e.what()};
  }

  Asset::SizeType const jsonSize{asset.json.size()};
  outputFileStream.write(asset.type, std::size(asset.type));
  outputFileStream.write(reinterpret_cast<char const*>(&jsonSize),
                         sizeof(jsonSize));

  Asset::SizeType const binaryBlobSize{asset.binaryBlob.size()};
  outputFileStream.write(reinterpret_cast<char const*>(&binaryBlobSize),
                         sizeof(binaryBlobSize));
  outputFileStream.write(asset.json.data(), asset.json.size());
  outputFileStream.write(asset.binaryBlob.data(), asset.binaryBlob.size());

  return {};
}

} /*namespace obsidian::asset*/
