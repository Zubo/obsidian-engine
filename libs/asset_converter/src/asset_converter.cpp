#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/globals/file_extensions.hpp>
#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <glm/glm.hpp>
#include <stb/stb_image.h>
#include <tiny_obj_loader.h>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace obsidian::asset_converter {

AssetConverter::AssetConverter(task::TaskExecutor& taskExecutor)
    : _taskExecutor{taskExecutor} {}

std::unordered_map<std::string, std::string> extensionMap = {
    {".bmp", globals::textureAssetExt}, {".jpeg", globals::textureAssetExt},
    {".jpg", globals::textureAssetExt}, {".png", globals::textureAssetExt},
    {".obj", globals::meshAssetExt},    {".spv", globals::shaderAssetExt}};

bool saveAsset(fs::path const& srcPath, fs::path const& dstPath,
               asset::Asset const& textureAsset) {
  ZoneScoped;

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

template <typename V>
std::size_t
generateVertices(tinyobj::attrib_t const& attrib,
                 std::vector<tinyobj::shape_t> const& shapes,
                 std::vector<char>& outVertices,
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

        glm::mat2x3 const edgeMtx = {facePositions[0] - facePositions[1],
                                     facePositions[2] - facePositions[1]};
        glm::vec2 const deltaUV0 = faceUVs[0] - faceUVs[1];
        glm::vec2 const deltaUV1 = faceUVs[2] - faceUVs[1];

        // factor for determinant of delta UV values matrix:
        float const factor =
            1 / (deltaUV0[0] * deltaUV1[1] - deltaUV1[0] * deltaUV0[1]);

        tangent = factor * edgeMtx * glm::vec2{deltaUV1[1], -deltaUV0[1]};
        tangent = glm::normalize(tangent);
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

std::optional<asset::TextureAssetInfo> AssetConverter::convertImgToAsset(
    fs::path const& srcPath, fs::path const& dstPath,
    std::optional<core::TextureFormat> overrideTextureFormat) {
  ZoneScoped;

  int w, h, channelCnt;

  unsigned char* data;

  {
    ZoneScopedN("STBI load");
    data = stbi_load(srcPath.c_str(), &w, &h, &channelCnt, 4);
  }

  bool const shouldAddAlpha = (channelCnt == 3);
  channelCnt = shouldAddAlpha ? channelCnt + 1 : channelCnt;
  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize = w * h * 4;
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format =
      overrideTextureFormat ? *overrideTextureFormat
                            : core::getDefaultFormatForChannelCount(channelCnt);

  if (textureAssetInfo.format == core::TextureFormat::unknown) {
    OBS_LOG_ERR("Failed to convert image to asset. Unsupported image format.");
    return std::nullopt;
  }

  textureAssetInfo.transparent = false;

  if (!shouldAddAlpha && channelCnt == 4) {
    ZoneScopedN("Transparency check");

    glm::vec4 const* pixels = reinterpret_cast<glm::vec4 const*>(data);
    for (glm::vec4 const* p = pixels; p < pixels + w * h; ++p) {
      if (p->a < 1.0f) {
        textureAssetInfo.transparent = true;
        break;
      }
    }
  }

  textureAssetInfo.width = w;
  textureAssetInfo.height = h;

  bool packResult;

  if (shouldAddAlpha) {
    ZoneScopedN("Adding alpha to img");

    // append alpha = 255 to all the pixels
    std::vector<unsigned char> imgWithAlpha;
    imgWithAlpha.resize(w * h * STBI_rgb_alpha);

    for (std::size_t i = 0; i < w * h; ++i) {
      for (std::size_t c = 0; c < 3; ++c) {
        imgWithAlpha[4 * i + c] = data[4 * i + c];
      }
      imgWithAlpha[4 * i + 3] = '\xFF';
    }

    packResult =
        asset::packTexture(textureAssetInfo, imgWithAlpha.data(), outAsset);
  } else {
    packResult = asset::packTexture(textureAssetInfo, data, outAsset);
  }

  {
    ZoneScopedN("STBI free");
    stbi_image_free(data);
  }

  if (!packResult) {
    return std::nullopt;
  }

  OBS_LOG_MSG("Successfully converted " + srcPath.string() +
              " to asset format.");
  if (saveAsset(srcPath, dstPath, outAsset)) {
    return textureAssetInfo;
  }

  return std::nullopt;
}

bool AssetConverter::convertObjToAsset(fs::path const& srcPath,
                                       fs::path const& dstPath) {
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
  std::vector<std::vector<core::MeshIndexType>> outSurfaces{
      materials.size() ? materials.size() : 1};
  std::size_t vertexCount;

  task::TaskBase const& genVertTask =
      _taskExecutor.enqueue(task::TaskType::general, [&]() {
        if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
            meshAssetInfo.hasUV) {
          vertexCount = generateVertices<core::VertexType<true, true, true>>(
              attrib, shapes, outVertices, outSurfaces);
        } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
          vertexCount = generateVertices<core::VertexType<true, true, false>>(
              attrib, shapes, outVertices, outSurfaces);
        } else if (meshAssetInfo.hasNormals) {
          vertexCount = generateVertices<core::VertexType<true, false, false>>(
              attrib, shapes, outVertices, outSurfaces);
        } else {
          vertexCount = generateVertices<core::VertexType<false, false, false>>(
              attrib, shapes, outVertices, outSurfaces);
        }
      });

  meshAssetInfo.defaultMatRelativePaths = extractMaterialsForObj(
      *this, srcPath.parent_path(), dstPath.parent_path(), materials);

  while (!genVertTask.getDone())
    ;

  meshAssetInfo.vertexCount = vertexCount;
  meshAssetInfo.vertexBufferSize = outVertices.size();
  meshAssetInfo.indexCount = 0;

  for (auto const& outSurface : outSurfaces) {
    meshAssetInfo.indexBufferSizes.push_back(sizeof(core::MeshIndexType) *
                                             outSurface.size());
    meshAssetInfo.indexCount += outSurface.size();
  }

  meshAssetInfo.defaultMatRelativePaths.resize(
      meshAssetInfo.indexBufferSizes.size());

  std::size_t const totalIndexBufferSize =
      std::accumulate(meshAssetInfo.indexBufferSizes.cbegin(),
                      meshAssetInfo.indexBufferSizes.cend(), 0);
  meshAssetInfo.unpackedSize =
      meshAssetInfo.vertexBufferSize + totalIndexBufferSize;
  outVertices.resize(outVertices.size() + totalIndexBufferSize);

  char* indCopyDest = outVertices.data() + meshAssetInfo.vertexBufferSize;

  for (std::size_t i = 0; i < outSurfaces.size(); ++i) {
    auto const& surface = outSurfaces[i];
    std::size_t const surfaceBufferSize = meshAssetInfo.indexBufferSizes[i];
    std::memcpy(indCopyDest, surface.data(), surfaceBufferSize);
    indCopyDest += surfaceBufferSize;
  }

  (void)indCopyDest;

  if (!asset::packMeshAsset(meshAssetInfo, std::move(outVertices), meshAsset)) {
    return false;
  }
  return saveAsset(srcPath, dstPath, meshAsset);
}

bool AssetConverter::convertSpirvToAsset(fs::path const& srcPath,
                                         fs::path const& dstPath) {
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

bool AssetConverter::convertAsset(fs::path const& srcPath,
                                  fs::path const& dstPath) {
  std::string const extension = srcPath.extension().string();

  if (!extension.size()) {
    OBS_LOG_ERR("Error: File doesn't have extension.");
    return false;
  }

  if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
      extension == ".bmp") {
    return convertImgToAsset(srcPath, dstPath).has_value();
  } else if (extension == ".obj") {
    return convertObjToAsset(srcPath, dstPath);
  } else if (extension == ".spv") {
    return convertSpirvToAsset(srcPath, dstPath);
  }

  OBS_LOG_ERR("Error: Unknown file extension.");
  return false;
}

std::optional<asset::TextureAssetInfo> AssetConverter::getOrCreateTexture(
    fs::path const& srcPath, fs::path const& dstPath,
    std::optional<core::TextureFormat> overrideTextureFormat) {
  if (std::filesystem::exists(dstPath)) {
    asset::Asset asset;
    if (!asset::loadAssetFromFile(dstPath, asset)) {
      return std::nullopt;
    }

    asset::TextureAssetInfo outInfo;
    if (!asset::readTextureAssetInfo(*asset.metadata, outInfo)) {
      return std::nullopt;
    }

    return {outInfo};
  } else {
    return convertImgToAsset(srcPath, dstPath, overrideTextureFormat);
  }
}

std::vector<std::string> AssetConverter::extractMaterialsForObj(
    AssetConverter& converter, fs::path const& srcDirPath,
    fs::path const& projectPath,
    std::vector<tinyobj::material_t> const& materials) {
  ZoneScoped;

  std::vector<std::string> extractedMaterialPaths;

  fs::directory_entry const texDir(srcDirPath / "textures");

  bool texDirExists = texDir.exists();

  std::unordered_map<std::string, task::TaskBase const*> textureLoadTasks;

  auto const getDiffuseTexName = [](tinyobj::material_t const& m) {
    return !m.diffuse_texname.empty() ? m.diffuse_texname : m.ambient_texname;
  };

  if (texDirExists) {
    for (std::size_t i = 0; i < materials.size(); ++i) {
      tinyobj::material_t const& mat = materials[i];

      std::string const diffuseTexName = getDiffuseTexName(mat);

      if (!diffuseTexName.empty() &&
          !textureLoadTasks.contains(diffuseTexName)) {
        fs::path const srcPath = texDir.path() / diffuseTexName;
        fs::path dstPath = projectPath / diffuseTexName;
        dstPath.replace_extension(".obstex");

        task::TaskBase const* task = &_taskExecutor.enqueue(
            task::TaskType::general, [this, srcPath, dstPath]() {
              return getOrCreateTexture(srcPath, dstPath);
            });

        textureLoadTasks[diffuseTexName] = task;
      }

      if (!mat.bump_texname.empty()) {
        fs::path const srcPath = texDir.path() / mat.bump_texname;
        fs::path dstPath = projectPath / mat.bump_texname;
        dstPath.replace_extension(".obstex");

        task::TaskBase const* task = &_taskExecutor.enqueue(
            task::TaskType::general, [this, srcPath, dstPath]() {
              return getOrCreateTexture(srcPath, dstPath,
                                        core::TextureFormat::R8G8B8A8_LINEAR);
            });

        textureLoadTasks[mat.bump_texname] = task;
      }
    }
  }

  // Wait for all texture tasks to finish
  while (true) {
    if (std::all_of(textureLoadTasks.cbegin(), textureLoadTasks.cend(),
                    [](auto const& it) { return it.second->getDone(); }))
      break;
  }

  extractedMaterialPaths.resize(materials.size());

  for (std::size_t i = 0; i < materials.size(); ++i) {
    tinyobj::material_t const& mat = materials[i];
    asset::MaterialAssetInfo newMatAssetInfo;
    newMatAssetInfo.compressionMode = asset::CompressionMode::none;
    newMatAssetInfo.materialType = core::MaterialType::lit;
    newMatAssetInfo.shaderPath = "obsidian/shaders/default.obsshad";
    newMatAssetInfo.ambientColor =
        glm::vec4(mat.ambient[0], mat.ambient[1], mat.ambient[2], mat.dissolve);

    newMatAssetInfo.diffuseColor =
        glm::vec4(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], mat.dissolve);

    newMatAssetInfo.specularColor =
        glm::vec4(mat.specular[0], mat.specular[1], mat.specular[2], 1.0f);

    newMatAssetInfo.shininess = mat.shininess;
    newMatAssetInfo.transparent = mat.dissolve < 1.0f;

    if (texDirExists) {
      std::string diffuseTexName = getDiffuseTexName(mat);
      if (!diffuseTexName.empty()) {
        std::optional<asset::TextureAssetInfo> const* texInfo =
            static_cast<std::optional<asset::TextureAssetInfo> const*>(
                textureLoadTasks[diffuseTexName]->getReturn().get());
        assert(texInfo);

        newMatAssetInfo.transparent |= (*texInfo && (*texInfo)->transparent);
        fs::path dstPath = diffuseTexName;
        dstPath.replace_extension(".obstex");
        newMatAssetInfo.diffuseTexturePath = dstPath;
      }

      if (!mat.bump_texname.empty()) {
        std::optional<asset::TextureAssetInfo> const* texInfo =
            static_cast<std::optional<asset::TextureAssetInfo> const*>(
                textureLoadTasks[mat.bump_texname]->getReturn().get());
        assert(texInfo);

        fs::path dstPath = mat.bump_texname;
        dstPath.replace_extension(".obstex");
        newMatAssetInfo.normalMapTexturePath = dstPath;
      }
    }

    asset::Asset matAsset;
    if (asset::packMaterial(newMatAssetInfo, {}, matAsset)) {
      fs::path materialPath = projectPath / mat.name;
      materialPath.replace_extension(".obsmat");

      if (asset::saveToFile(materialPath, matAsset)) {
        extractedMaterialPaths[i] = fs::relative(materialPath, projectPath);
      }
    }
  }

  return extractedMaterialPaths;
}

} /*namespace obsidian::asset_converter*/
