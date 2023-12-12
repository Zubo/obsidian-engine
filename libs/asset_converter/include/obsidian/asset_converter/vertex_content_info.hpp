#pragma once

#include <cstddef>

namespace obsidian::asset_converter {

struct VertexContentInfo {
  bool hasNormal;
  bool hasColor;
  bool hasUV;
  bool hasTangent;
};

inline std::size_t representAsInteger(VertexContentInfo const& v) {
  return v.hasNormal * (1 << 3) | v.hasColor * (1 << 2) | v.hasUV * (1 << 1) |
         v.hasTangent;
}

constexpr std::size_t intRepresentationMax() { return (1 << 4) - 1; }

} // namespace obsidian::asset_converter
