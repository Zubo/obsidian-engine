#pragma once

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <tiny_obj_loader.h>

namespace obsidian::asset_converter {

struct TinyGltfMaterialWrapper {
  tinygltf::Material const& mat;
  std::vector<tinygltf::Texture> const& textures;
};

inline std::string getMaterialName(tinyobj::material_t const& m) {
  return m.name;
}

inline std::string getMaterialName(TinyGltfMaterialWrapper const& m) {
  return m.mat.name;
}

inline std::string getDiffuseTexName(tinyobj::material_t const& m) {
  return !m.diffuse_texname.empty() ? m.diffuse_texname : m.ambient_texname;
}

inline std::string getDiffuseTexName(TinyGltfMaterialWrapper const& m) {
  int const index = m.mat.pbrMetallicRoughness.metallicRoughnessTexture.index;

  return index >= 0 ? m.textures[index].name : "";
}

inline std::string getNormalTexName(tinyobj::material_t const& m) {
  return m.bump_texname;
}

inline std::string getNormalTexName(TinyGltfMaterialWrapper const& m) {
  int const index = m.mat.normalTexture.index;

  return index >= 0 ? m.textures[index].name : "";
}

inline glm::vec4 getAmbientColor(tinyobj::material_t const& m) {
  return glm::vec4(m.ambient[0], m.ambient[1], m.ambient[2], m.dissolve);
}

inline glm::vec4 getAmbientColor(TinyGltfMaterialWrapper const& m) {
  return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

inline glm::vec4 getDiffuseColor(tinyobj::material_t const& m) {
  return glm::vec4(m.diffuse[0], m.diffuse[1], m.diffuse[2], m.dissolve);
}

inline glm::vec4 getDiffuseColor(TinyGltfMaterialWrapper const& m) {
  return glm::vec4(m.mat.pbrMetallicRoughness.baseColorFactor[0],
                   m.mat.pbrMetallicRoughness.baseColorFactor[1],
                   m.mat.pbrMetallicRoughness.baseColorFactor[2],
                   m.mat.alphaMode == "OPAQUE"
                       ? 1.0f
                       : m.mat.pbrMetallicRoughness.baseColorFactor[3]);
}

inline glm::vec4 getSpecularColor(tinyobj::material_t const& m) {
  return glm::vec4(m.specular[0], m.specular[1], m.specular[2], 1.0f);
}

inline glm::vec4 getSpecularColor(TinyGltfMaterialWrapper const& m) {
  return glm::vec4((1.0f - m.mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - m.mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - m.mat.pbrMetallicRoughness.roughnessFactor), 1.0f);
}

inline float getShininess(tinyobj::material_t const& m) { return m.shininess; }

inline float getShininess(TinyGltfMaterialWrapper const& m) {
  return 1.0f - m.mat.pbrMetallicRoughness.roughnessFactor;
}

inline bool isMaterialTransparent(tinyobj::material_t const& m) {
  return m.dissolve < 1.0f;
}

inline bool isMaterialTransparent(TinyGltfMaterialWrapper const& m) {
  return m.mat.alphaMode == "OPAQUE";
}

} /*namespace obsidian::asset_converter */
