#pragma once

#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/asset_converter/vertex_content_info.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <tiny_obj_loader.h>

#include <cstddef>
#include <string>

namespace obsidian::core {

struct Box3D;

} /*namespace obsidian::core */

namespace obsidian::asset_converter {

class ObjMeshWrapper {
public:
  ObjMeshWrapper();

private:
  std::vector<tinyobj::shape_t> const& _shapes;
  tinyobj::attrib_t const& _attribs;
};

void updateAabb(glm::vec3 pos, core::Box3D& aabb);

serialization::GameObjectData
nodeToGameObjectData(int nodeInd, tinygltf::Model const& model,
                     std::vector<std::string> const& meshPaths,
                     std::vector<asset::MeshAssetInfo> const& meshAssetInfos);

std::size_t callGenerateVerticesFromObj(
    asset::MeshAssetInfo const& meshAssetInfo, tinyobj::attrib_t const& attrib,
    std::vector<tinyobj::shape_t> const& shapes, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb);

std::size_t callGenerateVerticesFromGltfMesh(
    asset::MeshAssetInfo const& meshAssetInfo, tinygltf::Model const& model,
    int meshInd, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb);

struct GltfMaterialWrapper {
  tinygltf::Model const& model;
  int matInd;
  VertexContentInfo vertexInfo;
};

struct ObjMaterialWrapper {
  tinyobj::material_t const& mat;
  int matInd;
  VertexContentInfo vertexInfo;
};

inline std::string getMaterialName(ObjMaterialWrapper const& m,
                                   VertexContentInfo const& vertexInfo) {
  std::string result = m.mat.name;

  if (vertexInfo.hasNormal) {
    result += "_n";
  }
  if (vertexInfo.hasColor) {
    result += "_c";
  }
  if (vertexInfo.hasUV) {
    result += "_uv";
  }
  if (vertexInfo.hasTangent) {
    result += "_t";
  }

  return result;
}

inline std::string getMaterialName(GltfMaterialWrapper const& m,
                                   VertexContentInfo const& vertexInfo) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  std::string result = mat.name;

  if (vertexInfo.hasNormal) {
    result += "_n";
  }
  if (vertexInfo.hasColor) {
    result += "_c";
  }
  if (vertexInfo.hasUV) {
    result += "_uv";
  }
  if (vertexInfo.hasTangent) {
    result += "_t";
  }

  return result;
}

inline std::string getDiffuseTexName(ObjMaterialWrapper const& m) {
  return !m.mat.diffuse_texname.empty() ? m.mat.diffuse_texname
                                        : m.mat.ambient_texname;
}

inline std::string getDiffuseTexName(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  int const index = mat.pbrMetallicRoughness.baseColorTexture.index;

  if (index < 0) {
    return "";
  }

  int const source = m.model.textures[index].source;

  if (source < 0) {
    return "";
  }

  return m.model.images[source].uri;
}

template <typename MaterialWrapper>
inline std::string getAlbedoTexName(MaterialWrapper const& m) {
  return getDiffuseTexName(m);
}

inline std::string getNormalTexName(ObjMaterialWrapper const& m) {
  return !m.mat.normal_texname.empty() ? m.mat.normal_texname
                                       : m.mat.bump_texname;
}
inline std::string getNormalTexName(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  int const index = mat.normalTexture.index;

  if (index < 0) {
    return "";
  }

  int const source = m.model.textures[index].source;

  if (source < 0) {
    return "";
  }

  return m.model.images[source].uri;
  return index >= 0 ? m.model.textures[index].name : "";
}

inline std::string getMetalnessTexName(ObjMaterialWrapper const& m) {
  return m.mat.metallic_texname;
}

inline std::string getMetalnessTexName(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  int const index = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;

  if (index < 0) {
    return "";
  }

  int const source = m.model.textures[index].source;

  if (source < 0) {
    return "";
  }

  return m.model.images[source].uri;
}

inline bool isMetallicRoughnessTexSeparate(ObjMaterialWrapper const& m) {
  return true;
}

inline bool isMetallicRoughnessTexSeparate(GltfMaterialWrapper const& m) {
  return false;
}

inline std::string getRoughnessTexName(ObjMaterialWrapper const& m) {
  return m.mat.roughness_texname;
}

inline std::string getRoughnessTexName(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  int const index = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;

  if (index < 0) {
    return "";
  }

  int const source = m.model.textures[index].source;

  if (source < 0) {
    return "";
  }

  return m.model.images[source].uri;
}

inline VertexContentInfo getVertInfo(ObjMaterialWrapper const& m) {
  return m.vertexInfo;
}

inline VertexContentInfo getVertInfo(GltfMaterialWrapper const& m) {
  return m.vertexInfo;
}

inline int getMatInd(ObjMaterialWrapper const& m) { return m.matInd; }

inline int getMatInd(GltfMaterialWrapper const& m) { return m.matInd; }

inline glm::vec4 getAmbientColor(ObjMaterialWrapper const& m) {
  return glm::vec4(m.mat.ambient[0], m.mat.ambient[1], m.mat.ambient[2],
                   m.mat.dissolve);
}

inline glm::vec4 getAmbientColor(GltfMaterialWrapper const& m) {
  return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

inline glm::vec4 getDiffuseColor(ObjMaterialWrapper const& m) {
  return glm::vec4(m.mat.diffuse[0], m.mat.diffuse[1], m.mat.diffuse[2],
                   m.mat.dissolve);
}

inline glm::vec4 getDiffuseColor(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  return glm::vec4(mat.pbrMetallicRoughness.baseColorFactor[0],
                   mat.pbrMetallicRoughness.baseColorFactor[1],
                   mat.pbrMetallicRoughness.baseColorFactor[2],
                   mat.alphaMode == "OPAQUE"
                       ? 1.0f
                       : mat.pbrMetallicRoughness.baseColorFactor[3]);
}

inline glm::vec4 getSpecularColor(ObjMaterialWrapper const& m) {
  return glm::vec4(m.mat.specular[0], m.mat.specular[1], m.mat.specular[2],
                   1.0f);
}

inline glm::vec4 getSpecularColor(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  return glm::vec4((1.0f - mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - mat.pbrMetallicRoughness.roughnessFactor), 1.0f);
}

inline float getShininess(ObjMaterialWrapper const& m) {
  return m.mat.shininess;
}

inline float getShininess(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  // roughness factor: smooth - 0.0, rough - 1.0:
  return 1.0f + 511.0f * (1.0f - mat.pbrMetallicRoughness.roughnessFactor);
}

inline bool isMaterialTransparent(ObjMaterialWrapper const& m) {
  return m.mat.dissolve < 1.0f;
}

inline bool isMaterialTransparent(GltfMaterialWrapper const& m) {
  tinygltf::Material const& mat = m.model.materials[m.matInd];
  return mat.alphaMode != "OPAQUE";
}

std::string shaderPicker(VertexContentInfo const& vertexInfo,
                         core::MaterialType materialType);

std::string shaderPicker(GltfMaterialWrapper const& m,
                         core::MaterialType materialType);

std::string shaderPicker(ObjMaterialWrapper const& m,
                         core::MaterialType materialType);

} /*namespace obsidian::asset_converter */
