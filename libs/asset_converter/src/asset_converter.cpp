#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/texture_format.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <stb/stb_image.h>
#include <tiny_obj_loader.h>

#include <cstddef>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

namespace obsidian::asset_converter {

std::unordered_map<std::string, std::string> extensionMap = {
    {".png", ".obstex"}, {".obj", ".obsmesh"}, {".spv", ".obsshad"}};

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

bool convertPngToAsset(fs::path const& srcPath, fs::path const& dstPath) {
  int w, h, channelCnt;

  unsigned char* data =
      stbi_load(srcPath.c_str(), &w, &h, &channelCnt, STBI_rgb_alpha);

  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize = w * h * channelCnt;
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format = core::TextureFormat::R8G8B8A8;
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
std::size_t appendVec(V const& vec, char* dst, std::size_t offset) {
  std::memcpy(dst + offset, reinterpret_cast<char const*>(&vec), sizeof(vec));
  return offset + sizeof(vec);
}

bool convertObjToAsset(fs::path const& srcPath, fs::path const& dstPath) {
  asset::MeshAssetInfo meshAssetInfo;
  meshAssetInfo.compressionMode = asset::CompressionMode::LZ4;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warning, error;

  tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error,
                   srcPath.c_str(), "assets");

  if (!warning.empty()) {
    OBS_LOG_WARN("tinyobj warning: " + warning);
  }

  if (!error.empty()) {
    OBS_LOG_ERR("tinyobj error: " + error);
    return false;
  }

  meshAssetInfo.hasNormals = attrib.normals.size();
  meshAssetInfo.hasColors = attrib.colors.size();
  meshAssetInfo.hasUV = attrib.colors.size();
  meshAssetInfo.vertexCount = 0;
  std::size_t const vertexSize = asset::getVertexSize(meshAssetInfo);

  std::vector<char> meshData;

  for (std::size_t s = 0; s < shapes.size(); ++s) {
    tinyobj::shape_t const& shape = shapes[s];

    std::size_t faceIndOffset = 0;

    for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      unsigned char const vertexCount = shape.mesh.num_face_vertices[f];

      std::size_t blobOffset = meshData.size();
      meshData.resize(meshData.size() + vertexCount * vertexSize);

      for (std::size_t v = 0; v < vertexCount; ++v) {
        tinyobj::index_t const idx = shape.mesh.indices[faceIndOffset + v];

        ++meshAssetInfo.vertexCount;

        glm::vec3 const pos = {attrib.vertices[3 * idx.vertex_index + 0],
                               attrib.vertices[3 * idx.vertex_index + 1],
                               attrib.vertices[3 * idx.vertex_index + 2]};
        blobOffset = appendVec(pos, meshData.data(), blobOffset);

        if (meshAssetInfo.hasNormals) {
          glm::vec3 const normals = {attrib.normals[3 * idx.normal_index + 0],
                                     attrib.normals[3 * idx.normal_index + 1],
                                     attrib.normals[3 * idx.normal_index + 2]};
          blobOffset = appendVec(normals, meshData.data(), blobOffset);
        }

        if (meshAssetInfo.hasColors) {
          glm::vec3 const col = {attrib.colors[3 * idx.vertex_index + 0],
                                 attrib.colors[3 * idx.vertex_index + 1],
                                 attrib.colors[3 * idx.vertex_index + 2]};
          blobOffset = appendVec(col, meshData.data(), blobOffset);
        }

        if (meshAssetInfo.hasUV) {
          glm::vec2 const uv = {
              attrib.texcoords[2 * idx.texcoord_index + 0],
              1.f - attrib.texcoords[2 * idx.texcoord_index + 1]};
          blobOffset = appendVec(uv, meshData.data(), blobOffset);
        }
      }

      faceIndOffset += vertexCount;
    }
  }

  meshAssetInfo.unpackedSize = meshData.size();

  asset::Asset meshAsset;
  if (!asset::packMeshAsset(meshAssetInfo, std::move(meshData), meshAsset)) {
    return false;
  }

  return saveAsset(srcPath, dstPath, meshAsset);
} // namespace obsidian::asset_converter

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

  if (extension == ".png") {
    return convertPngToAsset(srcPath, dstPath);
  } else if (extension == ".obj") {
    return convertObjToAsset(srcPath, dstPath);
  } else if (extension == ".spv") {
    return convertSpirvToAsset(srcPath, dstPath);
  }

  OBS_LOG_ERR("Error: Unknown file extension.");
  return false;
}

} /*namespace obsidian::asset_converter*/
