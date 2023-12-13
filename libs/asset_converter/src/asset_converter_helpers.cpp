#include "glm/ext/scalar_constants.hpp"
#include <obsidian/asset_converter/asset_converter_helpers.hpp>

#include <glm/gtc/quaternion.hpp>

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

std::string shaderPicker(GltfMaterialWrapper const& m) {
  return shaderPicker(m.vertexInfo);
}

std::string shaderPicker(ObjMaterialWrapper const& m) {
  return shaderPicker(m.vertexInfo);
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

std::size_t callGenerateVerticesFromGltfMesh(
    asset::MeshAssetInfo const& meshAssetInfo, tinygltf::Model const& model,
    int meshInd, std::vector<char>& outVertices,
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

serialization::GameObjectData
nodeToGameObjectData(int nodeInd, tinygltf::Model const& model,
                     std::vector<std::string> const& meshPaths,
                     std::vector<asset::MeshAssetInfo> const& meshAssetInfos) {
  tinygltf::Node const& node = model.nodes[nodeInd];

  serialization::GameObjectData resultGameObjectData = {};

  resultGameObjectData.gameObjectName = node.name;

  if (node.mesh >= 0) {
    resultGameObjectData.meshPath = meshPaths[node.mesh];
    asset::MeshAssetInfo const& meshAssetInf = meshAssetInfos[node.mesh];

    resultGameObjectData.materialPaths = meshAssetInf.defaultMatRelativePaths;
  }

  if (node.translation.size()) {
    resultGameObjectData.position.x = node.translation[0];
    resultGameObjectData.position.y = node.translation[1];
    resultGameObjectData.position.z = node.translation[2];
  }

  if (node.rotation.size()) {
    glm::quat const q(node.rotation[3], node.rotation[0], node.rotation[1],
                      node.rotation[2]);
    resultGameObjectData.euler = glm::degrees(glm::eulerAngles(q));
  }

  if (node.scale.size()) {
    resultGameObjectData.scale.x = node.scale[0];
    resultGameObjectData.scale.y = node.scale[1];
    resultGameObjectData.scale.z = node.scale[2];
  }

  for (int childInd : node.children) {
    resultGameObjectData.children.push_back(
        nodeToGameObjectData(childInd, model, meshPaths, meshAssetInfos));
  }

  return resultGameObjectData;
}

} /*namespace obsidian::asset_converter*/
