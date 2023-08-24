#include <obsidian/asset/asset_io.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>

using namespace obsidian::runtime_resource;

RuntimeResource::RuntimeResource(std::filesystem::path path, rhi::RHI& rhi)
    : _path{std::move(path)}, _rhi{rhi} {}

bool RuntimeResource::loadAsset() {
  asset::Asset& asset = _asset.emplace();
  return asset::loadFromFile(_path, asset);
}

void RuntimeResource::releaseAsset() {
  if (_asset) {
    _asset.reset();
  }
}

void RuntimeResource::uploadToRHI() {}
