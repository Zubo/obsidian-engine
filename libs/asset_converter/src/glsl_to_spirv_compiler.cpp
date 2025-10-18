#include <obsidian/asset_converter/glsl_to_spirv_compiler.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/shader.hpp>

#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/DirStackFileIncluder.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <cassert>
#include <mutex>

using namespace obsidian::asset_converter;

std::mutex _glslangMutex;
std::int32_t shaderCompilerInstCount = 0;

GLSLToSpirvCompiler::GLSLToSpirvCompiler() {
  std::scoped_lock l{_glslangMutex};
  if (!shaderCompilerInstCount++) {
    glslang::InitializeProcess();
  }
}

GLSLToSpirvCompiler::~GLSLToSpirvCompiler() {
  std::scoped_lock l{_glslangMutex};
  if (!--shaderCompilerInstCount) {
    glslang::FinalizeProcess();
  }
  assert(shaderCompilerInstCount >= 0);
}

std::string getErrOutput(char const* infoLog, char const* infoDebugLog) {
  std::string err;
  err.reserve(std::strlen(infoLog) + strlen(infoDebugLog) + 1);
  err += infoLog;
  if (infoDebugLog) {
    err += "\n";
    err += infoDebugLog;
  }

  return err;
}

EShLanguage getShaderStage(obsidian::core::ShaderType shaderType) {
  if (shaderType == obsidian::core::ShaderType::vertex) {
    return EShLangVertex;
  }

  if (shaderType == obsidian::core::ShaderType::fragment) {
    return EShLangFragment;
  }

  OBS_LOG_ERR(std::string("Invalid shader type") +
              std::to_string(static_cast<int>(shaderType)));
  return EShLangCount;
}

bool GLSLToSpirvCompiler::compileShader(
    std::span<char> glslSrc, std::string shaderName,
    core::ShaderType shaderType, std::span<char const*> defines,
    std::filesystem::path const& shaderDir,
    std::vector<std::uint32_t>& outSpirvCode) const {
  outSpirvCode.clear();

  auto const shaderStage = getShaderStage(shaderType);

  if (shaderStage == EShLangCount) {
    return false;
  }

  glslang::TShader shader{shaderStage};

  std::string preamble;
  preamble.reserve(defines.size() * 25);

  for (char const* define : defines) {
    preamble += define;
    preamble += '\n';
  }

  shader.setPreamble(preamble.c_str());

  auto const cStrSrc = reinterpret_cast<char const*>(glslSrc.data());
  auto const cStrName = shaderName.c_str();
  int const size = glslSrc.size();
  shader.setStringsWithLengthsAndNames(&cStrSrc, &size, &cStrName,
                                       /*n sources=*/1);

  shader.setEnvInput(glslang::EShSourceGlsl, shaderStage,
                     glslang::EShClientVulkan, /*version=*/0);
  shader.setEnvClient(glslang::EShClientVulkan,
                      glslang::EShTargetVulkan_1_2); // TODO: Vulkan version
                                                     // should not be hardcoded
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

  DirStackFileIncluder includer;
  includer.pushExternalLocalDirectory(shaderDir);

  if (!shader.parse(GetDefaultResources(), 0, false, EShMsgDefault, includer)) {
    OBS_LOG_ERR(getErrOutput(shader.getInfoLog(), shader.getInfoDebugLog()));
    return false;
  }

  glslang::TProgram program;
  program.addShader(&shader);

  if (!program.link(EShMsgDefault)) {
    OBS_LOG_ERR(getErrOutput(program.getInfoLog(), program.getInfoDebugLog()));
    return false;
  }

  if constexpr (std::is_same_v<std::uint32_t, unsigned int>) {
    glslang::GlslangToSpv(*program.getIntermediate(shaderStage), outSpirvCode);
  } else {
    std::vector<unsigned int> buffer;
    glslang::GlslangToSpv(*program.getIntermediate(shaderStage), buffer);

    outSpirvCode.resize(buffer.size() * sizeof(unsigned int) /
                        sizeof(std::uint32_t));
    std::memcpy(outSpirvCode.data(), buffer.data(),
                outSpirvCode.size() * sizeof(outSpirvCode[0]));
  }

  return true;
}