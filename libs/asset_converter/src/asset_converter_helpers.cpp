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

std::size_t callGenerateVerticesFromObj(
    asset::MeshAssetInfo const& meshAssetInfo, tinyobj::attrib_t const& attrib,
    std::vector<tinyobj::shape_t> const& shapes, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces) {
  if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
      meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<true, true, true>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
    return generateVerticesFromObj<core::VertexType<true, true, false>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<true, false, true>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasColors && meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<false, true, true>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals) {
    return generateVerticesFromObj<core::VertexType<true, false, false>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasColors) {
    return generateVerticesFromObj<core::VertexType<false, true, false>>(
        attrib, shapes, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<false, false, true>>(
        attrib, shapes, outVertices, outSurfaces);
  } else {
    return generateVerticesFromObj<core::VertexType<false, false, false>>(
        attrib, shapes, outVertices, outSurfaces);
  }
};

std::size_t callGenerateVerticesFromGltf(
    asset::MeshAssetInfo const& meshAssetInfo, tinygltf::Model const& model,
    std::size_t meshInd, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces) {
  if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
      meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<true, true, true>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
    return generateVerticesFromGltf<core::VertexType<true, true, false>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<true, false, true>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasColors && meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<false, true, true>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasNormals) {
    return generateVerticesFromGltf<core::VertexType<true, false, false>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasColors) {
    return generateVerticesFromGltf<core::VertexType<false, true, false>>(
        model, meshInd, outVertices, outSurfaces);
  } else if (meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<false, false, true>>(
        model, meshInd, outVertices, outSurfaces);
  } else {
    return generateVerticesFromGltf<core::VertexType<false, false, false>>(
        model, meshInd, outVertices, outSurfaces);
  }
};

} /*namespace obsidian::asset_converter*/
