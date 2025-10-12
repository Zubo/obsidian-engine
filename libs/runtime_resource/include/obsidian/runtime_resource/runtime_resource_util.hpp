#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

namespace obsidian::runtime_resource {

rhi::UploadShaderRHI getUploadShader(obsidian::asset::Asset const& asset);

} /*namespace obsidian::runtime_resource*/