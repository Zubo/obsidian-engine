#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/asset_converter/asset_converter_helpers.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/shapes.hpp>
#include <obsidian/core/utils/aabb.hpp>

#include <glm/gtc/quaternion.hpp>
#include <tracy/Tracy.hpp>

#include <array>
#include <cassert>
#include <limits>
#include <numeric>
#include <unordered_map>

namespace obsidian::asset_converter {

std::string shaderPicker(VertexContentInfo const& vertexInfo,
                         core::MaterialType materialType) {
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

  switch (materialType) {
  case core::MaterialType::unlit:
    result += "default-unlit.obsshad";
    break;
  case core::MaterialType::lit:
    result += "default.obsshad";
    break;
  case core::MaterialType::pbr:
    result += "pbr.obsshad";
    break;
  default:
    assert(false &&
           "The materialType argument must either be MaterialType::lit or "
           "MaterialType::pbr.");
  }

  return result;
}

std::string shaderPicker(GltfMaterialWrapper const& m,
                         core::MaterialType materialType) {
  return shaderPicker(m.vertexInfo, materialType);
}

std::string shaderPicker(ObjMaterialWrapper const& m,
                         core::MaterialType materialType) {
  return shaderPicker(m.vertexInfo, materialType);
}

inline glm::vec3 calculateTangent(std::array<glm::vec3, 3> const& facePositions,
                                  std::array<glm::vec2, 3> const& faceUVs) {
  ZoneScoped;
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

inline std::uint32_t getUnsignedIntegerViaAccessor(tinygltf::Accessor const& a,
                                                   unsigned char const* ptr) {
  ZoneScoped;
  if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return *reinterpret_cast<std::uint16_t const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return *reinterpret_cast<std::uint32_t const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return *reinterpret_cast<std::int16_t const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_INT) {
    return *reinterpret_cast<std::int32_t const*>(ptr);
  }

  OBS_LOG_ERR("Expected component type to be any of the integer types.");
  assert(false);
  return 0;
}

inline float getFloatViaAccessor(tinygltf::Accessor const& a,
                                 unsigned char const* ptr) {
  ZoneScoped;
  if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return *reinterpret_cast<float const*>(ptr);
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return static_cast<float>(*reinterpret_cast<double const*>(ptr));
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return static_cast<float>(*reinterpret_cast<std::uint16_t const*>(ptr));
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return static_cast<float>(*reinterpret_cast<std::uint32_t const*>(ptr));
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return static_cast<float>(*reinterpret_cast<std::int16_t const*>(ptr));
  } else if (a.componentType == TINYGLTF_COMPONENT_TYPE_INT) {
    return static_cast<float>(*reinterpret_cast<std::int32_t const*>(ptr));
  }

  OBS_LOG_ERR("Component type expected to be either any floating point type or "
              "any integer type");

  return 0;
}

template <typename V>
std::size_t generateVerticesFromObj(
    tinyobj::attrib_t const& attrib,
    std::vector<tinyobj::shape_t> const& shapes, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb) {
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
  outAabb.bottomCorner = glm::vec3{std::numeric_limits<float>::infinity()};
  outAabb.topCorner = glm::vec3{-std::numeric_limits<float>::infinity()};

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

          core::utils::updateAabb(vertexPtr->pos, outAabb);

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

template <typename V>
std::size_t generateVerticesFromGltf(
    tinygltf::Model const& model, int meshInd, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb) {
  ZoneScoped;

  using Vertex = typename V::Vertex;

  assert(!outVertices.size() &&
         !std::accumulate(outSurfaces.cbegin(), outSurfaces.cend(), 0,
                          [](std::size_t acc, auto const& surfaceIndices) {
                            return surfaceIndices.size() + acc;
                          }) &&
         "Error: outVertices and outSurfaces have to be empty.");

  struct Ind {
    std::uint32_t vertexInd;
    unsigned char const* posDataPtr;
    unsigned char const* normalDataPtr;
    unsigned char const* uvDataPtr;
    glm::vec3 tangent = {};

    bool operator==(Ind const& other) const {
      constexpr const float tangentEpsilon = 0.001;
      return other.vertexInd == vertexInd && other.posDataPtr == posDataPtr &&
             other.normalDataPtr == normalDataPtr &&
             other.uvDataPtr == uvDataPtr &&
             (!V::hasTangent ||
              std::abs(1.0f - glm::dot(other.tangent, tangent)) <
                  tangentEpsilon);
    };

    struct hash {
      std::size_t operator()(Ind k) const { return k.vertexInd; }
    };
  };

  std::unordered_map<Ind, std::size_t, typename Ind::hash> uniqueIdx;
  outAabb.bottomCorner = glm::vec3{std::numeric_limits<float>::infinity()};
  outAabb.topCorner = glm::vec3{-std::numeric_limits<float>::infinity()};
  tinygltf::Mesh const& mesh = model.meshes[meshInd];
  std::unordered_map<int, std::size_t> materialToSurfaceIndMap;

  for (std::size_t primitiveInd = 0; primitiveInd < mesh.primitives.size();
       ++primitiveInd) {
    ZoneScopedN("GLTF primitive");

    tinygltf::Primitive const& primitive = mesh.primitives[primitiveInd];
    std::size_t surfaceInd;

    if (!materialToSurfaceIndMap.contains(primitive.material)) {
      surfaceInd = outSurfaces.size();
      materialToSurfaceIndMap[primitive.material] = surfaceInd;
      outSurfaces.push_back({});
    } else {
      surfaceInd = materialToSurfaceIndMap.at(primitive.material);
    }

    std::vector<core::MeshIndexType>& surface = outSurfaces[surfaceInd];

    tinygltf::Accessor const& indAccessor = model.accessors[primitive.indices];
    tinygltf::BufferView const& indBufferView =
        model.bufferViews[indAccessor.bufferView];
    tinygltf::Buffer const& indBuffer = model.buffers[indBufferView.buffer];
    unsigned char const* const indData = indBuffer.data.data() +
                                         indBufferView.byteOffset +
                                         indAccessor.byteOffset;
    std::size_t const indByteStride =
        indBufferView.byteStride
            ? indBufferView.byteStride
            : tinygltf::GetComponentSizeInBytes(indAccessor.componentType);

    tinygltf::Accessor const& posAccessor =
        model.accessors[primitive.attributes.at("POSITION")];
    tinygltf::BufferView const& posBufferView =
        model.bufferViews[posAccessor.bufferView];
    tinygltf::Buffer const& posBuffer = model.buffers[posBufferView.buffer];
    unsigned char const* const posData = posBuffer.data.data() +
                                         posBufferView.byteOffset +
                                         posAccessor.byteOffset;
    std::size_t const vertexPosSize =
        3 * tinygltf::GetComponentSizeInBytes(posAccessor.componentType);
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
      normalSize =
          3 * tinygltf::GetComponentSizeInBytes(normalAccessor->componentType);
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
      colorSize =
          3 * tinygltf::GetComponentSizeInBytes(colorAccessor->componentType);
      colorByteStride =
          colorBufferView->byteStride ? colorBufferView->byteStride : colorSize;
    }

    tinygltf::Accessor const* uvAccessor = nullptr;
    tinygltf::BufferView const* uvBufferView = nullptr;
    tinygltf::Buffer const* uvBuffer = nullptr;
    unsigned char const* uvBufferData = nullptr;
    std::size_t uvSize = 0;
    std::size_t uvByteStride = 0;

    if (V::hasUV) {
      uvAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
      uvBufferView = &model.bufferViews[uvAccessor->bufferView];
      uvBuffer = &model.buffers[uvBufferView->buffer];
      uvBufferData = uvBuffer->data.data() + uvBufferView->byteOffset +
                     uvAccessor->byteOffset;
      uvSize = 2 * tinygltf::GetComponentSizeInBytes(uvAccessor->componentType);
      uvByteStride =
          uvBufferView->byteStride ? uvBufferView->byteStride : uvSize;
    }

    for (std::size_t triangleInd = 0; triangleInd < indAccessor.count / 3;
         ++triangleInd) {
      std::array<std::uint32_t, 3> triangleIndices;

      for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
        triangleIndices[i] = getUnsignedIntegerViaAccessor(
            indAccessor, indData + (3 * triangleInd + i) * indByteStride);
      }

      glm::vec3 tangent = {};

      std::array<glm::vec3, 3> facePositions;
      std::array<glm::vec2, 3> faceUvs;

      for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
          facePositions[i][j] = getFloatViaAccessor(
              posAccessor, posData + (triangleIndices[i] * posByteStride +
                                      j * vertexPosSize / 3));
        }

        if constexpr (V::hasUV) {
          for (std::size_t j = 0; j < 2; ++j) {
            faceUvs[i][j] = getFloatViaAccessor(
                *uvAccessor, uvBufferData + (triangleIndices[i] * uvByteStride +
                                             j * uvSize / 2));
          }
        }
      }

      if constexpr (V::hasTangent) {
        tangent = calculateTangent(facePositions, faceUvs);
      }

      for (std::size_t i = 0; i < triangleIndices.size(); ++i) {
        std::uint32_t const vertInd = triangleIndices[i];

        decltype(uniqueIdx.emplace()) insertResult;

        insertResult = uniqueIdx.emplace(
            Ind{vertInd, posData, normalData, uvBufferData, tangent},
            outVertices.size() / sizeof(Vertex));

        if (insertResult.second) {
          // new vertex detected
          surface.push_back(outVertices.size() / sizeof(Vertex));

          std::size_t const newVertOffset = outVertices.size();
          {
            ZoneScopedN("Vertex buffer resizing");
            outVertices.resize(outVertices.size() + sizeof(Vertex));
          }

          Vertex* const vertexPtr =
              reinterpret_cast<Vertex*>(outVertices.data() + newVertOffset);

          vertexPtr->pos = facePositions[i];

          core::utils::updateAabb(vertexPtr->pos, outAabb);

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
            vertexPtr->uv = faceUvs[i]; //{faceUvs[i].x, 1.0f - faceUvs[i].y};
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

std::size_t callGenerateVerticesFromObj(
    asset::MeshAssetInfo const& meshAssetInfo, tinyobj::attrib_t const& attrib,
    std::vector<tinyobj::shape_t> const& shapes, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb) {
  if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
      meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<true, true, true>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
    return generateVerticesFromObj<core::VertexType<true, true, false>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<true, false, true>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasColors && meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<false, true, true>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals) {
    return generateVerticesFromObj<core::VertexType<true, false, false>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasColors) {
    return generateVerticesFromObj<core::VertexType<false, true, false>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasUV) {
    return generateVerticesFromObj<core::VertexType<false, false, true>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  } else {
    return generateVerticesFromObj<core::VertexType<false, false, false>>(
        attrib, shapes, outVertices, outSurfaces, outAabb);
  }
};

std::size_t callGenerateVerticesFromGltfMesh(
    asset::MeshAssetInfo const& meshAssetInfo, tinygltf::Model const& model,
    int meshInd, std::vector<char>& outVertices,
    std::vector<std::vector<core::MeshIndexType>>& outSurfaces,
    core::Box3D& outAabb) {
  if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
      meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<true, true, true>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
    return generateVerticesFromGltf<core::VertexType<true, true, false>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<true, false, true>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasColors && meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<false, true, true>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasNormals) {
    return generateVerticesFromGltf<core::VertexType<true, false, false>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasColors) {
    return generateVerticesFromGltf<core::VertexType<false, true, false>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else if (meshAssetInfo.hasUV) {
    return generateVerticesFromGltf<core::VertexType<false, false, true>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
  } else {
    return generateVerticesFromGltf<core::VertexType<false, false, false>>(
        model, meshInd, outVertices, outSurfaces, outAabb);
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
    resultGameObjectData.rotationQuat = q;
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
