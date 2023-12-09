#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/asset_converter/helpers.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/texture_format.hpp>
#include <obsidian/core/utils/texture_utils.hpp>
#include <obsidian/core/utils/utils.hpp>
#include <obsidian/core/vertex_type.hpp>
#include <obsidian/globals/file_extensions.hpp>
#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <glm/glm.hpp>
#include <stb/stb_image.h>
#include <tiny_gltf.h>
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

  constexpr int maxTextureSize = 1024;
  bool const reduceSize =
      (w > maxTextureSize) && core::isPowerOfTwo(w) && core::isPowerOfTwo(h);

  core::TextureFormat const textureFormat =
      overrideTextureFormat ? *overrideTextureFormat
                            : core::getDefaultFormatForChannelCount(channelCnt);

  std::vector<unsigned char> reducedImg;

  if (reduceSize) {
    ZoneScopedN("Image size reduction");

    int const reductionFactor = (w > h ? w : h) / maxTextureSize;
    bool const isTextureLinear = isFormatLinear(textureFormat);
    reducedImg = core::utils::reduceTextureSize(data, 4, w, h, reductionFactor,
                                                isTextureLinear);

    {
      ZoneScopedN("STBI free");
      stbi_image_free(data);
    }

    data = reducedImg.data();

    w = w / reductionFactor;
    h = h / reductionFactor;
  }

  bool const shouldAddAlpha = (channelCnt == 3);
  channelCnt = shouldAddAlpha ? channelCnt + 1 : channelCnt;
  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize = w * h * 4;
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format = textureFormat;

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

  if (!reduceSize) {
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
  ZoneScoped;

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
          vertexCount =
              generateVerticesFromObj<core::VertexType<true, true, true>>(
                  attrib, shapes, outVertices, outSurfaces);
        } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
          vertexCount =
              generateVerticesFromObj<core::VertexType<true, true, false>>(
                  attrib, shapes, outVertices, outSurfaces);
        } else if (meshAssetInfo.hasNormals) {
          vertexCount =
              generateVerticesFromObj<core::VertexType<true, false, false>>(
                  attrib, shapes, outVertices, outSurfaces);
        } else {
          vertexCount =
              generateVerticesFromObj<core::VertexType<false, false, false>>(
                  attrib, shapes, outVertices, outSurfaces);
        }
      });

  meshAssetInfo.defaultMatRelativePaths =
      extractMaterials(srcPath.parent_path(), dstPath.parent_path(), materials);

  while (!genVertTask.isDone())
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

bool AssetConverter::convertGltfToAsset(fs::path const& srcPath,
                                        fs::path const& dstPath) {
  ZoneScoped;

  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string err, warn;

  bool const loadResult =
      (srcPath.extension() == ".gltf" &&
       loader.LoadASCIIFromFile(&model, &err, &warn, srcPath)) ||
      (srcPath.extension() == ".glb" &&
       loader.LoadBinaryFromFile(&model, &err, &warn, srcPath));
  if (!loadResult) {
    if (!err.empty()) {
      OBS_LOG_ERR(err);
    }

    return false;
  }

  if (!warn.empty()) {
    OBS_LOG_WARN(warn);
  }

  std::vector<GltfMaterialWrapper> materials;
  materials.reserve(model.materials.size());

  for (std::size_t i = 0; i < model.materials.size(); ++i) {
    materials.push_back(
        GltfMaterialWrapper{model.materials[i], model.textures});
  }

  std::vector<std::string> materialNames =
      extractMaterials(srcPath.parent_path(), dstPath.parent_path(), materials);

  std::vector<task::TaskBase const*> genVerticesTasks;
  genVerticesTasks.reserve(model.meshes.size());

  std::vector<std::size_t> vertexCountPerMesh;
  vertexCountPerMesh.reserve(model.meshes.size());
  std::vector<std::vector<char>> outVerticesPerMesh;
  outVerticesPerMesh.reserve(model.meshes.size());
  std::vector<std::vector<std::vector<core::MeshIndexType>>> outSurfacesPerMesh;
  outSurfacesPerMesh.reserve(model.meshes.size());
  std::vector<asset::MeshAssetInfo> meshAssetInfoPerMesh;
  meshAssetInfoPerMesh.reserve(model.meshes.size());

  for (std::size_t i = 0; i < model.meshes.size(); ++i) {
    asset::MeshAssetInfo& meshAssetInfo = meshAssetInfoPerMesh.emplace_back();
    meshAssetInfo.compressionMode = asset::CompressionMode::LZ4;

    auto const& primitives = model.meshes[i].primitives;

    meshAssetInfo.hasNormals = std::all_of(
        primitives.cbegin(), primitives.cbegin(),
        [](auto const& p) { return p.attributes.contains("NORMAL"); });
    meshAssetInfo.hasColors =
        std::all_of(primitives.cbegin(), primitives.cend(), [](auto const& p) {
          return p.attributes.contains("COLOR_0");
        });
    meshAssetInfo.hasUV =
        std::all_of(primitives.cbegin(), primitives.cend(), [](auto const& p) {
          return p.attributes.contains("TEXCOORD_0");
        });

    task::TaskBase const& genVertTask = _taskExecutor.enqueue(
        task::TaskType::general,
        // capturing vector members by reference won't cause problems because
        // the vector memory was reserved in advance
        [&meshAssetInfo, &vertexCount = vertexCountPerMesh.emplace_back(),
         &model, meshInd = i, &outVertices = outVerticesPerMesh.emplace_back(),
         &outSurfaces = outSurfacesPerMesh.emplace_back()]() {
          outSurfaces.resize(model.materials.size());

          if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors &&
              meshAssetInfo.hasUV) {
            vertexCount =
                generateVerticesFromGltf<core::VertexType<true, true, true>>(
                    model, meshInd, outVertices, outSurfaces);
          } else if (meshAssetInfo.hasNormals && meshAssetInfo.hasColors) {
            vertexCount =
                generateVerticesFromGltf<core::VertexType<true, true, false>>(
                    model, meshInd, outVertices, outSurfaces);
          } else if (meshAssetInfo.hasNormals) {
            vertexCount =
                generateVerticesFromGltf<core::VertexType<true, false, false>>(
                    model, meshInd, outVertices, outSurfaces);
          } else {
            vertexCount =
                generateVerticesFromGltf<core::VertexType<false, false, false>>(
                    model, meshInd, outVertices, outSurfaces);
          }
        });

    genVerticesTasks.push_back(&genVertTask);
  }

  while (std::any_of(genVerticesTasks.cbegin(), genVerticesTasks.cend(),
                     [](task::TaskBase const* t) { return !t->isDone(); }))
    ;

  return true;
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
  } else if (extension == ".gltf" || extension == ".glb") {
    return convertGltfToAsset(srcPath, dstPath);
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

template <typename MaterialType>
std::vector<std::string>
AssetConverter::extractMaterials(fs::path const& srcDirPath,
                                 fs::path const& projectPath,
                                 std::vector<MaterialType> const& materials) {
  ZoneScoped;

  std::vector<std::string> extractedMaterialPaths;

  fs::directory_entry const texDir(srcDirPath / "textures");

  bool texDirExists = texDir.exists();

  std::unordered_map<std::string, task::TaskBase const*> textureLoadTasks;

  if (texDirExists) {
    for (std::size_t i = 0; i < materials.size(); ++i) {
      MaterialType const& mat = materials[i];

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

      std::string const normalTexName = getNormalTexName(mat);
      if (!normalTexName.empty()) {
        fs::path const srcPath = texDir.path() / normalTexName;
        fs::path dstPath = projectPath / normalTexName;
        dstPath.replace_extension(".obstex");

        task::TaskBase const* task = &_taskExecutor.enqueue(
            task::TaskType::general, [this, srcPath, dstPath]() {
              return getOrCreateTexture(srcPath, dstPath,
                                        core::TextureFormat::R8G8B8A8_LINEAR);
            });

        textureLoadTasks[normalTexName] = task;
      }
    }
  }

  // Wait for all texture tasks to finish
  while (true) {
    if (std::all_of(textureLoadTasks.cbegin(), textureLoadTasks.cend(),
                    [](auto const& it) { return it.second->isDone(); }))
      break;
  }

  extractedMaterialPaths.resize(materials.size());

  for (std::size_t i = 0; i < materials.size(); ++i) {
    MaterialType const& mat = materials[i];
    asset::MaterialAssetInfo newMatAssetInfo;
    newMatAssetInfo.compressionMode = asset::CompressionMode::none;
    newMatAssetInfo.materialType = core::MaterialType::lit;
    newMatAssetInfo.shaderPath = "obsidian/shaders/default.obsshad";
    newMatAssetInfo.ambientColor = getAmbientColor(mat);
    newMatAssetInfo.diffuseColor = getDiffuseColor(mat);
    newMatAssetInfo.specularColor = getSpecularColor(mat);

    newMatAssetInfo.shininess = getShininess(mat);
    newMatAssetInfo.transparent = isMaterialTransparent(mat);

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

      std::string const normalTexName = getNormalTexName(mat);
      if (!normalTexName.empty()) {
        std::optional<asset::TextureAssetInfo> const* texInfo =
            static_cast<std::optional<asset::TextureAssetInfo> const*>(
                textureLoadTasks[normalTexName]->getReturn().get());
        assert(texInfo);

        fs::path dstPath = normalTexName;
        dstPath.replace_extension(".obstex");
        newMatAssetInfo.normalMapTexturePath = dstPath;
      }
    }

    asset::Asset matAsset;
    if (asset::packMaterial(newMatAssetInfo, {}, matAsset)) {
      fs::path materialPath = projectPath / getMaterialName(mat);
      materialPath.replace_extension(".obsmat");

      if (asset::saveToFile(materialPath, matAsset)) {
        extractedMaterialPaths[i] = fs::relative(materialPath, projectPath);
      }
    }
  }

  return extractedMaterialPaths;
}

} /*namespace obsidian::asset_converter*/
