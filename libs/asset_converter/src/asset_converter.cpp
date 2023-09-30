#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/globals/file_extensions.hpp>

#include <glm/glm.hpp>
#include <stb/stb_image.h>
#include <tiny_obj_loader.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace obsidian::asset_converter {

std::unordered_map<std::string, std::string> extensionMap = {
    {".bmp", globals::textureAssetExt},
    {".jpg", globals::textureAssetExt},
    {".png", globals::textureAssetExt},
    {".obj", globals::meshAssetExt},
    {".spv", globals::shaderAssetExt}};

bool saveAsset(fs::path const& srcPath, fs::path const& dstPath,
               asset::Asset const& textureAsset) {
  if (!dstPath.has_extension()) {
    auto const extensionIter = extensionMap.find(srcPath.extension());

    if (extensionIter != extensionMap.cend()) {
      fs::path dstPathExt = dstPath;
      dstPathExt.replace_extension(extensionIter->second);
      return asset::saveToFile(dstPathExt, textureAsset);
    }
  }
  return asset::saveToFile(dstPath, textureAsset);
}

bool convertImgToAsset(fs::path const& srcPath, fs::path const& dstPath) {
  int w, h, channelCnt;

  unsigned char* data =
      stbi_load(srcPath.c_str(), &w, &h, &channelCnt, STBI_rgb_alpha);

  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize = w * h * channelCnt;
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format = core::getDefaultFormatForChannelCount(channelCnt);

  if (textureAssetInfo.format == core::TextureFormat::unknown) {
    OBS_LOG_ERR("Failed to convert image to asset. Unsupported image format.");
    return false;
  }

  textureAssetInfo.width = w;
  textureAssetInfo.height = h;

  bool const packResult = asset::packTexture(textureAssetInfo, data, outAsset);

  stbi_image_free(data);

  if (!packResult) {
    return false;
  }

  OBS_LOG_MSG("Successfully converted " + srcPath.string() +
              " to asset format.");
  return saveAsset(srcPath, dstPath, outAsset);
}

template <typename V>
std::size_t generateVertices(tinyobj::attrib_t const& attrib,
                             std::vector<tinyobj::shape_t> const& shapes,
                             std::vector<char>& outVertices,
                             std::vector<core::MeshIndexType>& outIndices) {
  using Vertex = typename V::Vertex;
  assert(!outVertices.size() && !outIndices.size() &&
         "Error: outVertices and outIndices have to be empty.");

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

        glm::mat2x3 const edgeMat = {facePositions[0] - facePositions[1],
                                     facePositions[2] - facePositions[1]};
        glm::vec2 const deltaUV0 = faceUVs[0] - faceUVs[1];
        glm::vec2 const deltaUV1 = faceUVs[2] - faceUVs[1];

        // factor for determinant of delta UV values matrix:
        float const f =
            1 / (deltaUV0[0] * deltaUV1[1] - deltaUV1[0] * deltaUV0[1]);

        tangent = f * edgeMat * glm::vec2{deltaUV1[1], -deltaUV0[1]};
        tangent = glm::normalize(tangent);
      }

      for (std::size_t v = 0; v < vertexCount; ++v) {
        tinyobj::index_t const idx = shape.mesh.indices[faceIndOffset + v];

        auto insertResult =
            uniqueIdx.emplace(Ind{idx.vertex_index, idx.normal_index,
                                  idx.texcoord_index, tangent},
                              outVertices.size() / sizeof(Vertex));

        if (insertResult.second) {
          // New vertex detected
          outIndices.push_back(outVertices.size() / sizeof(Vertex));
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
          outIndices.push_back(insertResult.first->second);
        }
      }

      faceIndOffset += vertexCount;
    }
  }

  return outVertices.size() / sizeof(Vertex);
}

bool convertObjToAsset(fs::path const& srcPath, fs::path const& dstPath) {
  asset::MeshAssetInfo meshAssetInfo;
  meshAssetInfo.compressionMode = asset::CompressionMode::LZ4;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warning, error;

  fs::path const srcDirPath = srcPath.parent_path();
  tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error,
                   srcPath.c_str(), srcDirPath.c_str());

  if (!warning.empty()) {
    OBS_LOG_WARN("tinyobj warning: " + warning);
  }

  if (!error.empty()) {
    OBS_LOG_ERR("tinyobj error: " + error);
    return false;
  }

  meshAssetInfo.hasNormals = attrib.normals.size();
  meshAssetInfo.hasColors = attrib.colors.size();
  meshAssetInfo.hasUV = attrib.texcoords.size();

  asset::Asset meshAsset;

  std::vector<char> outVertices;
  std::vector<core::MeshIndexType> outIndices;

  std::size_t vertexCount;
  if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
      meshAssetInfo.hasUV) {
    vertexCount = generateVertices<core::VertexType<true, true, true>>(
        attrib, shapes, outVertices, outIndices);
  } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
    vertexCount = generateVertices<core::VertexType<true, true, false>>(
        attrib, shapes, outVertices, outIndices);
  } else if (meshAssetInfo.hasNormals) {
    vertexCount = generateVertices<core::VertexType<true, false, false>>(
        attrib, shapes, outVertices, outIndices);
  } else {
    vertexCount = generateVertices<core::VertexType<false, false, false>>(
        attrib, shapes, outVertices, outIndices);
  }

  meshAssetInfo.vertexCount = vertexCount;
  meshAssetInfo.vertexBufferSize = outVertices.size();
  meshAssetInfo.indexCount = outIndices.size();
  meshAssetInfo.indexBufferSize =
      sizeof(core::MeshIndexType) * meshAssetInfo.indexCount;
  meshAssetInfo.unpackedSize =
      meshAssetInfo.vertexBufferSize + meshAssetInfo.indexBufferSize;

  outVertices.resize(outVertices.size() + meshAssetInfo.indexBufferSize);
  std::memcpy(outVertices.data() + meshAssetInfo.vertexBufferSize,
              outIndices.data(), meshAssetInfo.indexBufferSize);

  if (!asset::packMeshAsset(meshAssetInfo, std::move(outVertices), meshAsset)) {
    return false;
  }
  return saveAsset(srcPath, dstPath, meshAsset);
}

bool convertSpirvToAsset(fs::path const& srcPath, fs::path const& dstPath) {
  std::ifstream file{srcPath, std::ios::ate | std::ios::binary};

  if (!file.is_open()) {
    return false;
  }

  std::size_t const fileSize = static_cast<std::size_t>(file.tellg());

  std::vector<char> buffer(fileSize);

  file.seekg(0);

  file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

  file.close();

  asset::Asset shaderAsset;
  asset::ShaderAssetInfo shaderAssetInfo;
  shaderAssetInfo.unpackedSize = buffer.size();
  shaderAssetInfo.compressionMode = asset::CompressionMode::none;

  bool const packResult =
      asset::packShader(shaderAssetInfo, std::move(buffer), shaderAsset);

  if (!packResult) {
    return false;
  }

  OBS_LOG_MSG("Successfully converted " + srcPath.string() +
              " to asset format.");
  return saveAsset(srcPath, dstPath, shaderAsset);
}

bool convertAsset(fs::path const& srcPath, fs::path const& dstPath) {
  std::string const extension = srcPath.extension().string();

  if (!extension.size()) {
    OBS_LOG_ERR("Error: File doesn't have extension.");
    return false;
  }

  if (extension == ".png" || extension == ".jpg" || extension == ".bmp") {
    return convertImgToAsset(srcPath, dstPath);
  } else if (extension == ".obj") {
    return convertObjToAsset(srcPath, dstPath);
  } else if (extension == ".spv") {
    return convertSpirvToAsset(srcPath, dstPath);
  }

  OBS_LOG_ERR("Error: Unknown file extension.");
  return false;
}

} /*namespace obsidian::asset_converter*/
