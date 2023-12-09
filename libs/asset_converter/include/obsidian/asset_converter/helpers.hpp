#pragma once

#include <obsidian/core/logging.hpp>
#include <obsidian/core/vertex_type.hpp>

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <tiny_obj_loader.h>
#include <tracy/Tracy.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <numeric>
#include <unordered_map>

namespace obsidian::asset_converter {

class ObjMeshWrapper {
public:
  ObjMeshWrapper();

private:
  std::vector<tinyobj::shape_t> const& _shapes;
  tinyobj::attrib_t const& _attribs;
};

inline glm::vec3 calculateTangent(std::array<glm::vec3, 3> const& facePositions,
                                  std::array<glm::vec2, 3> const& faceUVs) {
  glm::mat2x3 const edgeMtx = {facePositions[0] - facePositions[1],
                               facePositions[2] - facePositions[1]};
  glm::vec2 const deltaUV0 = faceUVs[0] - faceUVs[1];
  glm::vec2 const deltaUV1 = faceUVs[2] - faceUVs[1];

  // factor for determinant of delta UV values matrix:
  float const factor =
      1 / (deltaUV0[0] * deltaUV1[1] - deltaUV1[0] * deltaUV0[1]);

  glm::vec3 tangent = factor * edgeMtx * glm::vec2{deltaUV1[1], -deltaUV0[1]};
  tangent = glm::normalize(tangent);

  return tangent;
}

template <typename V>
std::size_t generateVerticesFromObj(
    tinyobj::attrib_t const& attrib,
    std::vector<tinyobj::shape_t> const& shapes, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces) {
  ZoneScoped;

  using Vertex = typename V::Vertex;

  assert(!outVertices.size() &&
         !std::accumulate(outSurfaces.cbegin(), outSurfaces.cend(), 0,
                          [](std::size_t acc, auto const& surfaceIndices) {
                            return surfaceIndices.size() + acc;
                          }) &&
         "Error: outVertices and outSurfaces have to be empty.");

  struct Ind {
    int v;
    int n;
    int t;
    glm::vec3 tangent = {};

    bool operator==(Ind const& other) const {
      constexpr const float tangentEpsilon = 0.001;
      return other.v == v && other.n == n && other.t == t &&
             (!V::hasTangent ||
              std::abs(1.0f - glm::dot(other.tangent, tangent)) <
                  tangentEpsilon);
    };

    struct hash {
      std::size_t operator()(Ind k) const {
        return ((std::uint64_t)k.v << 42) | ((std::uint64_t)k.n << 21) | k.t;
      }
    };
  };

  std::unordered_map<Ind, std::size_t, typename Ind::hash> uniqueIdx;

  for (std::size_t s = 0; s < shapes.size(); ++s) {
    tinyobj::shape_t const& shape = shapes[s];

    std::size_t faceIndOffset = 0;

    for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      unsigned char const vertexCount = shape.mesh.num_face_vertices[f];

      glm::vec3 tangent;

      if constexpr (V::hasTangent) {
        std::array<glm::vec3, 3> facePositions = {};
        std::array<glm::vec2, 3> faceUVs = {};

        for (std::size_t i = 0; i < 3; ++i) {
          tinyobj::index_t const idx = shape.mesh.indices[faceIndOffset + i];
          facePositions[i] = {attrib.vertices[3 * idx.vertex_index + 0],
                              attrib.vertices[3 * idx.vertex_index + 1],
                              attrib.vertices[3 * idx.vertex_index + 2]};

          faceUVs[i] = {attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.f - attrib.texcoords[2 * idx.texcoord_index + 1]};
        }

        tangent = calculateTangent(facePositions, faceUVs);
      }

      for (std::size_t v = 0; v < vertexCount; ++v) {
        tinyobj::index_t const idx = shape.mesh.indices[faceIndOffset + v];

        auto insertResult =
            uniqueIdx.emplace(Ind{idx.vertex_index, idx.normal_index,
                                  idx.texcoord_index, tangent},
                              outVertices.size() / sizeof(Vertex));
        int const matInd =
            outSurfaces.size() > 1 ? shape.mesh.material_ids[f] : 0;

        if (insertResult.second) {
          // New vertex detected
          outSurfaces[matInd].push_back(outVertices.size() / sizeof(Vertex));
          std::size_t const newVertOffset = outVertices.size();
          outVertices.resize(outVertices.size() + sizeof(Vertex));

          Vertex* const vertexPtr =
              reinterpret_cast<Vertex*>(outVertices.data() + newVertOffset);

          vertexPtr->pos = {attrib.vertices[3 * idx.vertex_index + 0],
                            attrib.vertices[3 * idx.vertex_index + 1],
                            attrib.vertices[3 * idx.vertex_index + 2]};

          if constexpr (V::hasNormal) {
            vertexPtr->normal = {attrib.normals[3 * idx.normal_index + 0],
                                 attrib.normals[3 * idx.normal_index + 1],
                                 attrib.normals[3 * idx.normal_index + 2]};
          }

          if constexpr (V::hasColor) {
            vertexPtr->color = {attrib.colors[3 * idx.vertex_index + 0],
                                attrib.colors[3 * idx.vertex_index + 1],
                                attrib.colors[3 * idx.vertex_index + 2]};
          }

          if constexpr (V::hasUV) {
            vertexPtr->uv = {attrib.texcoords[2 * idx.texcoord_index + 0],
                             1.f -
                                 attrib.texcoords[2 * idx.texcoord_index + 1]};
          }

          if constexpr (V::hasTangent) {
            vertexPtr->tangent = tangent;
          }

        } else {
          // Insert the index of already added vertex
          outSurfaces[matInd].push_back(insertResult.first->second);
        }
      }

      faceIndOffset += vertexCount;
    }
  }

  return outVertices.size() / sizeof(Vertex);
}

inline std::uint32_t getIntegerViaAccessor(tinygltf::Accessor const& a,
                                           unsigned char const* ptr) {
  if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return *reinterpret_cast<std::uint16_t const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return *reinterpret_cast<std::uint32_t const*>(ptr);
  }

  OBS_LOG_ERR("Expected component type to be either "
              "TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT or "
              "TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT.");
  assert(false);
  return 0;
}

inline float getFloatViaAccessor(tinygltf::Accessor const& a,
                                 unsigned char const* ptr) {
  if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return *reinterpret_cast<float const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return static_cast<float>(*reinterpret_cast<double const*>(ptr));
  }

  OBS_LOG_ERR("Expected component type to be either "
              "TINYGLTF_COMPONENT_TYPE_FLOAT or "
              "TINYGLTF_COMPONENT_TYPE_DOUBLE.");

  return 0;
}

template <typename V>
std::size_t generateVerticesFromGltf(
    tinygltf::Model const& model, std::size_t meshInd,
    std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces) {
  using Vertex = typename V::Vertex;

  assert(!outVertices.size() &&
         !std::accumulate(outSurfaces.cbegin(), outSurfaces.cend(), 0,
                          [](std::size_t acc, auto const& surfaceIndices) {
                            return surfaceIndices.size() + acc;
                          }) &&
         "Error: outVertices and outSurfaces have to be empty.");

  struct Ind {
    std::uint32_t vertexInd;
    int vBufferInd;
    int nBufferInd;
    int tBufferInd;
    glm::vec3 tangent = {};

    bool operator==(Ind const& other) const {
      constexpr const float tangentEpsilon = 0.001;
      return other.vertexInd == vertexInd && other.vBufferInd == vBufferInd &&
             other.nBufferInd == nBufferInd && other.tBufferInd == tBufferInd &&
             (!V::hasTangent ||
              std::abs(1.0f - glm::dot(other.tangent, tangent)) <
                  tangentEpsilon);
    };

    struct hash {
      std::size_t operator()(Ind k) const {
        return ((std::uint64_t)k.vertexInd << 32) |
               ((std::uint64_t)(k.vBufferInd | k.nBufferInd | k.tBufferInd));
      }
    };
  };

  std::unordered_map<Ind, std::size_t, typename Ind::hash> uniqueIdx;

  tinygltf::Mesh const& mesh = model.meshes[meshInd];

  for (std::size_t primitiveInd = 0; primitiveInd < mesh.primitives.size();
       ++primitiveInd) {
    tinygltf::Primitive const& primitive = mesh.primitives[primitiveInd];

    int const matInd = outSurfaces.size() == 1 ? 0 : primitive.material;
    std::vector<core::MeshIndexType>& surface = outSurfaces[matInd];

    tinygltf::Accessor const& indAccessor = model.accessors[primitive.indices];
    tinygltf::BufferView const& indBufferView =
        model.bufferViews[indAccessor.bufferView];
    tinygltf::Buffer const& indBuffer = model.buffers[indBufferView.buffer];
    unsigned char const* const indData = indBuffer.data.data() +
                                         indBufferView.byteOffset +
                                         indAccessor.byteOffset;
    std::size_t const indByteStride =
        indBufferView.byteStride ? indBufferView.byteStride
                                 : indBufferView.byteLength / indAccessor.count;

    tinygltf::Accessor const& posAccessor =
        model.accessors[primitive.attributes.at("POSITION")];
    tinygltf::BufferView const& posBufferView =
        model.bufferViews[posAccessor.bufferView];
    tinygltf::Buffer const& posBuffer = model.buffers[posBufferView.buffer];
    unsigned char const* const posData = posBuffer.data.data() +
                                         posBufferView.byteOffset +
                                         posAccessor.byteOffset;
    std::size_t const vertexPosSize =
        posBufferView.byteLength / posAccessor.count;
    std::size_t const posByteStride =
        posBufferView.byteStride ? posBufferView.byteStride : vertexPosSize;

    tinygltf::Accessor const* normalAccessor = nullptr;
    tinygltf::BufferView const* normalBufferView = nullptr;
    tinygltf::Buffer const* normalBuffer = nullptr;
    unsigned char const* normalData = nullptr;
    std::size_t normalSize = 0;
    std::size_t normalByteStride = 0;

    if constexpr (V::hasNormal) {
      normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
      normalBufferView = &model.bufferViews[normalAccessor->bufferView];
      normalBuffer = &model.buffers[normalBufferView->buffer];
      normalData = normalBuffer->data.data() + normalBufferView->byteOffset +
                   normalAccessor->byteOffset;
      normalSize = normalBufferView->byteLength / normalAccessor->count;
      normalByteStride = normalBufferView->byteStride
                             ? normalBufferView->byteStride
                             : normalSize;
    }

    tinygltf::Accessor const* colorAccessor = nullptr;
    tinygltf::BufferView const* colorBufferView = nullptr;
    tinygltf::Buffer const* colorBuffer = nullptr;
    unsigned char const* colorData = nullptr;
    std::size_t colorSize = 0;
    std::size_t colorByteStride = 0;

    if constexpr (V::hasColor) {
      colorAccessor = &model.accessors[primitive.attributes.at("COLOR_0")];
      colorBufferView = &model.bufferViews[colorAccessor->bufferView];
      colorBuffer = &model.buffers[colorBufferView->buffer];
      colorData = colorBuffer->data.data() + colorBufferView->byteOffset +
                  colorAccessor->byteOffset;
      colorSize = colorBufferView->byteLength / colorAccessor->count;
      colorByteStride =
          colorBufferView->byteStride ? colorBufferView->byteStride : colorSize;
    }

    tinygltf::Accessor const* uvAccessor;
    tinygltf::BufferView const* uvBufferView;
    tinygltf::Buffer const* uvBuffer;
    unsigned char const* uvBufferData = nullptr;
    std::size_t uvSize = 0;
    std::size_t uvByteStride = 0;

    if (V::hasUV) {
      uvAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
      uvBufferView = &model.bufferViews[uvAccessor->bufferView];
      uvBuffer = &model.buffers[uvBufferView->buffer];
      uvBufferData = uvBuffer->data.data() + uvBufferView->byteOffset +
                     uvAccessor->byteOffset;
      uvSize = uvBufferView->byteLength / uvAccessor->count;
      uvByteStride =
          uvBufferView->byteStride ? uvBufferView->byteStride : uvSize;
    }

    for (std::size_t triangleInd = 0; triangleInd < indAccessor.count / 3;
         ++triangleInd) {
      std::array<std::uint32_t, 3> triangleIndices;

      for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
        triangleIndices[i] = getIntegerViaAccessor(
            indAccessor, indData + (3 * triangleInd + i) * indByteStride);
      }

      glm::vec3 tangent = {};

      std::array<glm::vec3, 3> facePositions;
      std::array<glm::vec2, 3> faceUvs;

      if constexpr (V::hasTangent) {
        for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
          for (std::size_t j = 0; j < 3; ++j) {
            facePositions[i][j] = getFloatViaAccessor(
                posAccessor, posData + (triangleIndices[i] * posByteStride +
                                        j * vertexPosSize / 3));
          }

          for (std::size_t j = 0; j < 2; ++j) {
            faceUvs[i][j] = getFloatViaAccessor(
                *uvAccessor, uvBufferData + (triangleIndices[i] * uvByteStride +
                                             j * uvSize / 2));
          }
        }

        tangent = calculateTangent(facePositions, faceUvs);
      }

      for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
        std::uint32_t const vertInd = triangleIndices[i];

        triangleIndices[i] = vertInd;

        auto const insertResult = uniqueIdx.emplace(
            Ind{vertInd, posBufferView.buffer,
                normalBufferView ? normalBufferView->buffer : 0,
                uvBufferView ? uvBufferView->buffer : 0, tangent},
            outVertices.size() / sizeof(Vertex));

        if (insertResult.second) {
          // new vertex detected
          surface.push_back(outVertices.size() / sizeof(Vertex));

          std::size_t const newVertOffset = outVertices.size();
          outVertices.resize(outVertices.size() + sizeof(Vertex));

          Vertex* const vertexPtr =
              reinterpret_cast<Vertex*>(outVertices.data() + newVertOffset);

          vertexPtr->pos = facePositions[i];

          if constexpr (V::hasNormal) {
            for (std::size_t i = 0; i < 3; ++i) {
              vertexPtr->normal[i] = getFloatViaAccessor(
                  *normalAccessor, normalData + (vertInd * normalByteStride +
                                                 i * normalSize / 3));
            }
          }

          if constexpr (V::hasColor) {
            for (std::size_t i = 0; i < 3; ++i) {
              vertexPtr->color[i] = getFloatViaAccessor(
                  *colorAccessor,
                  colorData + (vertInd * colorByteStride + i * colorSize / 3));
            }
          }

          if constexpr (V::hasUV) {
            vertexPtr->uv = faceUvs[i];
          }

          if constexpr (V::hasTangent) {
            vertexPtr->tangent = tangent;
          }

        } else {
          surface.emplace_back(insertResult.first->second);
        }
      }
    }
  }

  return outVertices.size() / sizeof(Vertex);
}

struct GltfMaterialWrapper {
  tinygltf::Material const& mat;
  std::vector<tinygltf::Texture> const& textures;
};

inline std::string getMaterialName(tinyobj::material_t const& m) {
  return m.name;
}

inline std::string getMaterialName(GltfMaterialWrapper const& m) {
  return m.mat.name;
}

inline std::string getDiffuseTexName(tinyobj::material_t const& m) {
  return !m.diffuse_texname.empty() ? m.diffuse_texname : m.ambient_texname;
}

inline std::string getDiffuseTexName(GltfMaterialWrapper const& m) {
  int const index = m.mat.pbrMetallicRoughness.metallicRoughnessTexture.index;

  return index >= 0 ? m.textures[index].name : "";
}

inline std::string getNormalTexName(tinyobj::material_t const& m) {
  return m.bump_texname;
}

inline std::string getNormalTexName(GltfMaterialWrapper const& m) {
  int const index = m.mat.normalTexture.index;

  return index >= 0 ? m.textures[index].name : "";
}

inline glm::vec4 getAmbientColor(tinyobj::material_t const& m) {
  return glm::vec4(m.ambient[0], m.ambient[1], m.ambient[2], m.dissolve);
}

inline glm::vec4 getAmbientColor(GltfMaterialWrapper const& m) {
  return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

inline glm::vec4 getDiffuseColor(tinyobj::material_t const& m) {
  return glm::vec4(m.diffuse[0], m.diffuse[1], m.diffuse[2], m.dissolve);
}

inline glm::vec4 getDiffuseColor(GltfMaterialWrapper const& m) {
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

inline glm::vec4 getSpecularColor(GltfMaterialWrapper const& m) {
  return glm::vec4((1.0f - m.mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - m.mat.pbrMetallicRoughness.roughnessFactor),
                   (1.0f - m.mat.pbrMetallicRoughness.roughnessFactor), 1.0f);
}

inline float getShininess(tinyobj::material_t const& m) { return m.shininess; }

inline float getShininess(GltfMaterialWrapper const& m) {
  return 1.0f - m.mat.pbrMetallicRoughness.roughnessFactor;
}

inline bool isMaterialTransparent(tinyobj::material_t const& m) {
  return m.dissolve < 1.0f;
}

inline bool isMaterialTransparent(GltfMaterialWrapper const& m) {
  return m.mat.alphaMode == "OPAQUE";
}

} /*namespace obsidian::asset_converter */
