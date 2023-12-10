#include <obsidian/asset_converter/asset_converter_helpers.hpp>

namespace obsidian::asset_converter {

std::string shaderPicker(VertexContentInfo const& vertexInfo) {
  std::string result = "obsidian/shaders/";

  if (vertexInfo.hasColor) {
    result += "c";
  }
  if (vertexInfo.hasUV) {
    result += "u";
  }

  if (vertexInfo.hasColor || vertexInfo.hasUV) {
    result += "-";
  }

  if (vertexInfo.hasNormal) {
    result += "default.obsshad";
  } else {
    result += "default-unlit.obsshad";
  }

  return result;
}

} /*namespace obsidian::asset_converter*/
