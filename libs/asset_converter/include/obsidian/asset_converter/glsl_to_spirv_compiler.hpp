#pragma once

#include <obsidian/core/shader.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace obsidian::asset_converter {

class GLSLToSpirvCompiler {
  GLSLToSpirvCompiler(GLSLToSpirvCompiler const&) = delete;
  GLSLToSpirvCompiler(GLSLToSpirvCompiler&&) = delete;
  GLSLToSpirvCompiler& operator=(GLSLToSpirvCompiler const&) = delete;
  GLSLToSpirvCompiler& operator=(GLSLToSpirvCompiler&&) = delete;

public:
  GLSLToSpirvCompiler();
  ~GLSLToSpirvCompiler();

  bool compileShader(std::span<char> glslSrc, std::string shaderName,
                     core::ShaderType shaderType,
                     std::span<char const*> defines,
                     std::filesystem::path const& shaderDir,
                     std::vector<std::uint32_t>& outSpirvCode) const;
};

} /*namespace obsidian::asset_converter*/