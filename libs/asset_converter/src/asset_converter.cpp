#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/prefab_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset_converter/asset_converter.hpp>
#include <obsidian/asset_converter/asset_converter_helpers.hpp>
#include <obsidian/asset_converter/vertex_content_info.hpp>
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
#include <nlohmann/json.hpp>
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
    {".obj", globals::meshAssetExt},    {".gltf", globals::meshAssetExt},
    {".glb", globals::meshAssetExt},    {".spv", globals::shaderAssetExt}};

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
    fs::path const& srcPath, fs::path const& dstPath, bool generateMips,
    std::optional<core::TextureFormat> overrideTextureFormat) {
  ZoneScoped;

  int w, h, fileChannelCnt;

  constexpr const int channelCnt = 4;

  unsigned char* stbiImgData = nullptr;

  {
    ZoneScopedN("STBI load");
    stbiImgData =
        stbi_load(srcPath.c_str(), &w, &h, &fileChannelCnt, channelCnt);
  }

  unsigned char* data = stbiImgData;

  if (!data) {
    OBS_LOG_ERR("Failed to load image with path: " + srcPath.string());
    return std::nullopt;
  }

  constexpr int maxTextureSize = 1024;
  bool const reduceSize =
      (w > maxTextureSize) && core::isPowerOfTwo(w) && core::isPowerOfTwo(h);

  bool const willGenerateMips =
      generateMips && core::isPowerOfTwo(w) && core::isPowerOfTwo(h);

  if (generateMips && !willGenerateMips) {
    OBS_LOG_WARN("Mips won't be generated because the texture dimensions are "
                 "not power of two.");
  }

  core::TextureFormat const textureFormat =
      overrideTextureFormat
          ? *overrideTextureFormat
          : core::getDefaultFormatForChannelCount(fileChannelCnt);

  std::vector<unsigned char> modifiedImageBuffer;
  std::size_t resultW = w;
  std::size_t resultH = h;

  std::size_t const nonLinearChannelCnt =
      core::numberOfNonLinearChannels(textureFormat);

  if (reduceSize) {
    ZoneScopedN("Image size reduction");

    int const reductionFactor = (w > h ? w : h) / maxTextureSize;

    resultW = w / reductionFactor;
    resultH = h / reductionFactor;
    modifiedImageBuffer.resize(resultW * resultH * channelCnt *
                               (willGenerateMips ? 2 : 1));

    core::utils::reduceTextureSize(data, modifiedImageBuffer.data(), channelCnt,
                                   w, h, reductionFactor, nonLinearChannelCnt);

    data = modifiedImageBuffer.data();
  }

  std::size_t mipLevels = 1;

  if (generateMips) {
    if (!reduceSize) {
      modifiedImageBuffer.resize(resultW * resultH * channelCnt * 2);
      std::memcpy(modifiedImageBuffer.data(), data,
                  modifiedImageBuffer.size() / 2);
      data = modifiedImageBuffer.data();
    }

    std::size_t srcLevelW = resultW;
    std::size_t srcLevelH = resultH;
    std::size_t srcOffset = 0;
    std::size_t dstOffset = srcLevelW * srcLevelH * channelCnt;

    while ((srcLevelW > 1) && (srcLevelH > 1)) {
      core::utils::reduceTextureSize(data + srcOffset, data + dstOffset,
                                     channelCnt, srcLevelW, srcLevelH, 2,
                                     nonLinearChannelCnt);

      srcOffset += srcLevelW * srcLevelH * channelCnt;

      srcLevelW >>= 1;
      srcLevelH >>= 1;

      dstOffset += srcLevelW * srcLevelH * channelCnt;

      ++mipLevels;
    }
  }

  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize =
      resultW * resultH * channelCnt * (willGenerateMips ? 2 : 1);
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format = textureFormat;

  if (textureAssetInfo.format == core::TextureFormat::unknown) {
    OBS_LOG_ERR("Failed to convert image to asset. Unsupported image format.");
    return std::nullopt;
  }

  textureAssetInfo.transparent = false;

  bool const alphaPresentInImgFile = fileChannelCnt == channelCnt;
  if (alphaPresentInImgFile) {
    ZoneScopedN("Transparency check");

    glm::u8vec4 const* pixels = reinterpret_cast<glm::u8vec4 const*>(data);
    for (glm::u8vec4 const* p = pixels; p < pixels + w * h; ++p) {
      if (p->a < 0xff) {
        textureAssetInfo.transparent = true;
        break;
      }
    }
  }

  textureAssetInfo.width = resultW;
  textureAssetInfo.height = resultH;
  textureAssetInfo.mipLevels = mipLevels;

  bool const packResult = asset::packTexture(textureAssetInfo, data, outAsset);

  {
    ZoneScopedN("STBI free");
    stbi_image_free(stbiImgData);
  }

  if (!packResult) {
    return std::nullopt;
  }

  if (saveAsset(srcPath, dstPath, outAsset)) {
    OBS_LOG_MSG("Successfully converted " + srcPath.string() +
                " to asset format.");
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
  meshAssetInfo.hasTangents = meshAssetInfo.hasUV && meshAssetInfo.hasNormals;

  std::vector<char> outVertices;
  std::vector<std::vector<core::MeshIndexType>> outSurfaces{
      materials.size() ? materials.size() : 1};
  std::size_t vertexCount;

  task::TaskBase const& genVertTask =
      _taskExecutor.enqueue(task::TaskType::general, [&]() {
        vertexCount = callGenerateVerticesFromObj(meshAssetInfo, attrib, shapes,
                                                  outVertices, outSurfaces,
                                                  meshAssetInfo.aabb);
      });

  VertexContentInfo const vertInfo = {
      meshAssetInfo.hasNormals, meshAssetInfo.hasColors, meshAssetInfo.hasUV,
      meshAssetInfo.hasTangents};

  std::vector<ObjMaterialWrapper> requestedMaterials;
  requestedMaterials.reserve(materials.size());

  for (std::size_t i = 0; i < materials.size(); ++i) {
    requestedMaterials.push_back({materials[i], (int)i, vertInfo});
  }

  fs::path const projectPath = dstPath.parent_path();

  TextureAssetInfoMap const texAssetInfoMap = extractTexturesForMaterials(
      srcDirPath, projectPath, requestedMaterials, true);

  MaterialPathTable const extractedMaterials =
      extractMaterials(srcPath.parent_path(), dstPath.parent_path(),
                       texAssetInfoMap, requestedMaterials, materials.size());

  meshAssetInfo.defaultMatRelativePaths.reserve(materials.size());

  for (std::string const& path :
       extractedMaterials[representAsInteger(vertInfo)]) {
    meshAssetInfo.defaultMatRelativePaths.push_back(path);
  }

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

  asset::Asset meshAsset;

  if (!asset::packMeshAsset(meshAssetInfo, std::move(outVertices), meshAsset)) {
    OBS_LOG_ERR("Failed to convert " + srcPath.string() + " to asset.");
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

  {
    ZoneScopedN("GLTF load from file");

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
  }

  if (!warn.empty()) {
    OBS_LOG_WARN(warn);
  }

  std::size_t meshCount = model.meshes.size();

  std::vector<std::size_t> vertexCountPerMesh;
  vertexCountPerMesh.resize(meshCount, 0);
  std::vector<std::vector<char>> outVerticesPerMesh;
  outVerticesPerMesh.resize(meshCount);
  std::vector<std::vector<std::vector<core::MeshIndexType>>> outSurfacesPerMesh{
      meshCount};
  std::vector<asset::MeshAssetInfo> meshAssetInfoPerMesh;
  meshAssetInfoPerMesh.resize(meshCount);

  std::vector<task::TaskBase const*> generateVerticesTasks;
  generateVerticesTasks.reserve(meshCount);

  for (std::size_t i = 0; i < model.meshes.size(); ++i) {
    asset::MeshAssetInfo& meshAssetInfo = meshAssetInfoPerMesh[i];
    meshAssetInfo.compressionMode = asset::CompressionMode::LZ4;

    meshAssetInfo.hasNormals = std::all_of(
        model.meshes[i].primitives.cbegin(), model.meshes[i].primitives.cend(),
        [](auto const& p) { return p.attributes.contains("NORMAL"); });
    meshAssetInfo.hasColors = std::all_of(
        model.meshes[i].primitives.cbegin(), model.meshes[i].primitives.cend(),
        [](auto const& p) { return p.attributes.contains("COLOR_0"); });
    meshAssetInfo.hasUV = std::all_of(
        model.meshes[i].primitives.cbegin(), model.meshes[i].primitives.cend(),
        [](auto const& p) { return p.attributes.contains("TEXCOORD_0"); });

    meshAssetInfo.hasTangents = meshAssetInfo.hasNormals && meshAssetInfo.hasUV;

    task::TaskBase const& generateVerticesTask = _taskExecutor.enqueue(
        task::TaskType::general,
        // capturing vector members by reference won't cause problems because
        // the vector memory was reserved in advance
        [meshInd = i, &meshAssetInfo, &vertexCount = vertexCountPerMesh[i],
         &model, &outVertices = outVerticesPerMesh[i],
         &outSurfaces = outSurfacesPerMesh[i]]() {
          std::size_t const materialCnt = model.materials.size();
          vertexCount = callGenerateVerticesFromGltfMesh(
              meshAssetInfo, model, meshInd, outVertices, outSurfaces,
              meshAssetInfo.aabb);
        });

    generateVerticesTasks.push_back(&generateVerticesTask);
  }

  std::vector<GltfMaterialWrapper> requestedMaterials;

  std::vector<std::vector<int>> materialIndicesPerMesh;
  materialIndicesPerMesh.resize(meshCount);

  for (std::size_t meshInd = 0; meshInd < meshCount; ++meshInd) {
    asset::MeshAssetInfo const& meshAssetInfo = meshAssetInfoPerMesh[meshInd];
    VertexContentInfo vertInfo = {meshAssetInfo.hasNormals,
                                  meshAssetInfo.hasColors, meshAssetInfo.hasUV,
                                  meshAssetInfo.hasTangents};

    for (tinygltf::Primitive const& primitive :
         model.meshes[meshInd].primitives) {
      if (primitive.material < 0) {
        continue;
      }

      requestedMaterials.push_back({model, primitive.material, vertInfo});
      materialIndicesPerMesh[meshInd].push_back(primitive.material);
    }
  }

  fs::path const projectPath = dstPath.parent_path();
  fs::path const srcDirPath = srcPath.parent_path();

  TextureAssetInfoMap const texAssetInfoMap = extractTexturesForMaterials(
      srcDirPath, projectPath, requestedMaterials, false);

  MaterialPathTable extractedMaterialPaths =
      extractMaterials(srcPath, projectPath, texAssetInfoMap,
                       requestedMaterials, model.materials.size());

  while (std::any_of(generateVerticesTasks.cbegin(),
                     generateVerticesTasks.cend(),
                     [](task::TaskBase const* t) { return !t->isDone(); }))
    ;

  bool exportSuccess = true;

  std::vector<std::string> meshExportPaths;
  meshExportPaths.reserve(meshCount);

  for (std::size_t i = 0; i < meshCount; ++i) {
    asset::MeshAssetInfo& meshAssetInfo = meshAssetInfoPerMesh[i];
    meshAssetInfo.vertexCount = vertexCountPerMesh[i];

    std::vector<char>& outVertices = outVerticesPerMesh[i];

    meshAssetInfo.vertexBufferSize = outVerticesPerMesh[i].size();
    meshAssetInfo.indexCount = 0;

    meshAssetInfo.defaultMatRelativePaths.reserve(
        meshAssetInfo.indexBufferSizes.size());

    std::vector<std::vector<core::MeshIndexType>>& outSurfaces =
        outSurfacesPerMesh[i];

    std::size_t const vertInfoInt =
        representAsInteger({meshAssetInfo.hasNormals, meshAssetInfo.hasColors,
                            meshAssetInfo.hasUV, meshAssetInfo.hasTangents});

    std::vector<int> const& materialIndices = materialIndicesPerMesh[i];

    for (std::size_t j = 0; j < outSurfaces.size(); ++j) {
      meshAssetInfo.indexBufferSizes.push_back(sizeof(core::MeshIndexType) *
                                               outSurfaces[j].size());
      meshAssetInfo.indexCount += outSurfaces[j].size();

      if (materialIndices.size()) {
        int const matInd = materialIndices[j];
        meshAssetInfo.defaultMatRelativePaths.push_back(
            extractedMaterialPaths[vertInfoInt][matInd]);
      }
    }

    std::size_t const totalIndexBufferSize =
        std::accumulate(meshAssetInfo.indexBufferSizes.cbegin(),
                        meshAssetInfo.indexBufferSizes.cend(), 0);
    meshAssetInfo.unpackedSize =
        meshAssetInfo.vertexBufferSize + totalIndexBufferSize;
    outVertices.resize(outVertices.size() + totalIndexBufferSize);

    char* indCopyDest = outVertices.data() + meshAssetInfo.vertexBufferSize;

    for (std::size_t j = 0; j < outSurfaces.size(); ++j) {
      auto const& surface = outSurfaces[j];
      std::size_t const surfaceBufferSize = meshAssetInfo.indexBufferSizes[j];
      std::memcpy(indCopyDest, surface.data(), surfaceBufferSize);
      indCopyDest += surfaceBufferSize;
    }

    (void)indCopyDest;

    asset::Asset meshAsset;

    if (!asset::packMeshAsset(meshAssetInfo, std::move(outVertices),
                              meshAsset)) {
      exportSuccess = false;
      break;
    }

    tinygltf::Mesh const& mesh = model.meshes[i];
    std::string& exportpath = meshExportPaths.emplace_back(
        dstPath.string() + (mesh.name.empty()
                                ? std::to_string(i)
                                : mesh.name + globals::meshAssetExt));

    if (!saveAsset(srcPath, exportpath, meshAsset)) {
      exportSuccess = false;
      break;
    }
  }

  if (!exportSuccess) {
    OBS_LOG_ERR("Failed to convert " + srcPath.string() + " to asset.");
    return false;
  }

  std::vector<std::string> meshRelativePaths;
  std::transform(meshExportPaths.cbegin(), meshExportPaths.cend(),
                 std::back_inserter(meshRelativePaths),
                 [dstPath](std::string const& pathStr) {
                   return fs::path(pathStr).lexically_relative(
                       dstPath.parent_path());
                 });

  for (std::size_t sceneInd = 0; sceneInd < model.scenes.size(); ++sceneInd) {
    ZoneScopedN("GLTF prefab export");

    tinygltf::Scene const& scene = model.scenes[sceneInd];

    for (int nodeInd : scene.nodes) {
      serialization::GameObjectData rootNodeObjData = nodeToGameObjectData(
          nodeInd, model, meshRelativePaths, meshAssetInfoPerMesh);

      nlohmann::json gameObjectJson;

      if (serialization::serializeGameObject(rootNodeObjData, gameObjectJson)) {
        std::string gameObjectJsonString = gameObjectJson.dump();
        std::vector<char> prefabData;
        prefabData.resize(gameObjectJsonString.size());
        std::memcpy(prefabData.data(), gameObjectJsonString.data(),
                    gameObjectJsonString.size());

        asset::PrefabAssetInfo prefabAssetInfo;
        prefabAssetInfo.compressionMode = asset::CompressionMode::LZ4;
        prefabAssetInfo.unpackedSize = gameObjectJsonString.size();

        asset::Asset prefabAsset;
        if (!asset::packPrefab(prefabAssetInfo, std::move(prefabData),
                               prefabAsset)) {
          OBS_LOG_ERR("Failed packing scene on index " +
                      std::to_string(sceneInd) + std::to_string(nodeInd));
          continue;
        }

        tinygltf::Node const& node = model.nodes[nodeInd];
        fs::path prefabPath =
            dstPath.string() +
            (node.name.empty() ? std::to_string(nodeInd) : node.name);

        prefabPath.replace_extension(globals::prefabAssetExt);

        if (!asset::saveToFile(prefabPath, prefabAsset)) {
          OBS_LOG_ERR("Failed saving prefab to path " + prefabPath.string());
        }
      }
    }
  }

  return true;
} // namespace obsidian::asset_converter

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
    OBS_LOG_ERR("Failed to convert " + srcPath.string() + " to asset format.");
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
    return convertImgToAsset(srcPath, dstPath, true).has_value();
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

void AssetConverter::setMaterialType(core::MaterialType matType) {
  _materialType = matType;
}

std::optional<asset::TextureAssetInfo> AssetConverter::getOrImportTexture(
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
    return convertImgToAsset(srcPath, dstPath, true, overrideTextureFormat);
  }
}

template <typename MaterialType>
AssetConverter::TextureAssetInfoMap AssetConverter::extractTexturesForMaterials(
    fs::path const& srcDirPath, fs::path const& projectPath,
    std::vector<MaterialType> const& materials, bool tryFindingTextureSubdir) {
  ZoneScoped;

  fs::directory_entry texDir;

  if (tryFindingTextureSubdir) {
    texDir = fs::directory_entry(srcDirPath / "textures");
  }

  if (!tryFindingTextureSubdir || !texDir.exists()) {
    texDir = fs::directory_entry{srcDirPath};
  }

  std::unordered_map<std::string, task::TaskBase const*> textureLoadTasks;

  auto const addTex = [this, &textureLoadTasks, &texDir, &projectPath](
                          std::string texName, core::TextureFormat texFormat) {
    if (texName.empty() || textureLoadTasks.contains(texName)) {
      return;
    }

    fs::path const srcPath = texDir.path() / texName;
    fs::path dstPath = projectPath / texName;
    dstPath.replace_extension(globals::textureAssetExt);

    task::TaskBase const* task = &_taskExecutor.enqueue(
        task::TaskType::general, [this, srcPath, dstPath, texFormat]() {
          return getOrImportTexture(srcPath, dstPath, texFormat);
        });

    textureLoadTasks[texName] = task;
  };

  for (std::size_t i = 0; i < materials.size(); ++i) {
    MaterialType const& mat = materials[i];

    addTex(getDiffuseTexName(mat), core::TextureFormat::R8G8B8A8_SRGB);
    addTex(getNormalTexName(mat), core::TextureFormat::R8G8B8A8_LINEAR);
    addTex(getMetalnessTexName(mat), core::TextureFormat::R8G8B8A8_LINEAR);
    addTex(getRoughnessTexName(mat), core::TextureFormat::R8G8B8A8_LINEAR);
  }

  // Wait for all texture tasks to finish
  while (true) {
    if (std::all_of(textureLoadTasks.cbegin(), textureLoadTasks.cend(),
                    [](auto const& it) { return it.second->isDone(); }))
      break;
  }

  TextureAssetInfoMap resultTextureInfos;

  for (auto const& t : textureLoadTasks) {
    std::optional<asset::TextureAssetInfo> texInfoOpt =
        *reinterpret_cast<std::optional<asset::TextureAssetInfo> const*>(
            t.second->getReturn());
    resultTextureInfos[t.first] = texInfoOpt;
  }

  return resultTextureInfos;
}

template <typename MaterialType>
void extractUnlitMaterialData(
    MaterialType const& mat, asset::MaterialAssetInfo& outMaterialAssetInfo,
    AssetConverter::TextureAssetInfoMap const& textureAssetInfoMap) {
  outMaterialAssetInfo.materialType = core::MaterialType::unlit;

  asset::UnlitMaterialAssetData& unlitMatAssetData =
      outMaterialAssetInfo.materialSubtypeData
          .emplace<asset::UnlitMaterialAssetData>();

  unlitMatAssetData.color = getDiffuseColor(mat);

  std::string const colorTexName = getDiffuseTexName(mat);
  if (!colorTexName.empty()) {
    std::optional<asset::TextureAssetInfo> const& colorTexInfo =
        textureAssetInfoMap.at(colorTexName);
    assert(colorTexInfo);

    outMaterialAssetInfo.transparent |=
        (colorTexInfo && colorTexInfo->transparent);
    fs::path dstPath = colorTexName;
    dstPath.replace_extension(globals::textureAssetExt);
    unlitMatAssetData.colorTexturePath = dstPath;
  }
}

template <typename MaterialType>
void extractLitMaterialData(
    MaterialType const& mat, asset::MaterialAssetInfo& outMaterialAssetInfo,
    AssetConverter::TextureAssetInfoMap const& textureAssetInfoMap) {
  outMaterialAssetInfo.materialType = core::MaterialType::lit;

  asset::LitMaterialAssetData& litMatAssetData =
      outMaterialAssetInfo.materialSubtypeData
          .emplace<asset::LitMaterialAssetData>();

  litMatAssetData.ambientColor = getAmbientColor(mat);
  litMatAssetData.diffuseColor = getDiffuseColor(mat);
  litMatAssetData.specularColor = getSpecularColor(mat);
  litMatAssetData.shininess = getShininess(mat);
  litMatAssetData.reflection = false;

  std::string const diffuseTexName = getDiffuseTexName(mat);
  if (!diffuseTexName.empty()) {
    std::optional<asset::TextureAssetInfo> const& texInfo =
        textureAssetInfoMap.at(diffuseTexName);
    assert(texInfo);

    outMaterialAssetInfo.transparent |= (texInfo && texInfo->transparent);
    fs::path dstPath = diffuseTexName;
    dstPath.replace_extension(globals::textureAssetExt);
    litMatAssetData.diffuseTexturePath = dstPath;
  }

  std::string const normalTexName = getNormalTexName(mat);
  if (!normalTexName.empty()) {
    std::optional<asset::TextureAssetInfo> const& texInfo =
        textureAssetInfoMap.at(normalTexName);
    assert(texInfo);

    fs::path dstPath = normalTexName;
    dstPath.replace_extension(globals::textureAssetExt);
    litMatAssetData.normalMapTexturePath = dstPath;
  }
}

template <typename MaterialType>
void extractPbrOrFallbackMaterialData(
    MaterialType const& mat, asset::MaterialAssetInfo& outMaterialAssetInfo,
    AssetConverter::TextureAssetInfoMap const& textureAssetInfoMap) {
  asset::PBRMaterialAssetData& pbrMatAssetData =
      outMaterialAssetInfo.materialSubtypeData
          .emplace<asset::PBRMaterialAssetData>();

  std::string const albedoTexName = getAlbedoTexName(mat);
  std::string const normalTexName = getNormalTexName(mat);
  std::string const metalnessTexName = getMetalnessTexName(mat);

  if (albedoTexName.empty() || normalTexName.empty() ||
      metalnessTexName.empty()) {
    // fallback
    OBS_LOG_WARN(
        "Missing textures - pbr pipeline requires following textures: albedo, "
        "normal and metal/roughness (in one texture as RG channels or as "
        "separate textures). Falling back to lit material pipeline. Material "
        "name: " +
        getMaterialShortName(mat) + ". ");

    outMaterialAssetInfo.materialType = core::MaterialType::lit;
    extractLitMaterialData(mat, outMaterialAssetInfo, textureAssetInfoMap);
    return;
  }

  outMaterialAssetInfo.materialType = core::MaterialType::pbr;

  // albedo
  std::optional<asset::TextureAssetInfo> const& albedoTexInfo =
      textureAssetInfoMap.at(albedoTexName);
  assert(albedoTexInfo);

  outMaterialAssetInfo.transparent |=
      (albedoTexInfo && albedoTexInfo->transparent);

  fs::path albedoDstPath = albedoTexName;
  albedoDstPath.replace_extension(globals::textureAssetExt);
  pbrMatAssetData.albedoTexturePath = albedoDstPath;

  // normal map
  assert(!normalTexName.empty() && "Normal map texture missing.");
  std::optional<asset::TextureAssetInfo> const& normalMapTexInfo =
      textureAssetInfoMap.at(normalTexName);
  assert(normalMapTexInfo);

  fs::path normalMapDstPath = normalTexName;
  normalMapDstPath.replace_extension(globals::textureAssetExt);
  pbrMatAssetData.normalMapTexturePath = normalMapDstPath;

  // metalness
  assert(!metalnessTexName.empty() && "Normal map texture missing.");
  std::optional<asset::TextureAssetInfo> const& metalnessTexInfo =
      textureAssetInfoMap.at(metalnessTexName);
  assert(metalnessTexInfo);

  fs::path metalnessDstPath = metalnessTexName;
  metalnessDstPath.replace_extension(globals::textureAssetExt);
  pbrMatAssetData.metalnessTexturePath = metalnessDstPath;

  // roughness
  if (isMetallicRoughnessTexSeparate(mat)) {
    std::string const roughnessTexName = getRoughnessTexName(mat);
    assert(!roughnessTexName.empty() && "Normal map texture missing.");
    std::optional<asset::TextureAssetInfo> const& roughnessTexInfo =
        textureAssetInfoMap.at(roughnessTexName);
    assert(roughnessTexInfo);

    fs::path roughnessDstPath = roughnessTexName;
    roughnessDstPath.replace_extension(globals::textureAssetExt);
    pbrMatAssetData.roughnessTexturePath = roughnessDstPath;
  }
}

template <typename MaterialType>
AssetConverter::MaterialPathTable
AssetConverter::extractMaterials(fs::path const& srcDirPath,
                                 fs::path const& projectPath,
                                 TextureAssetInfoMap const& textureAssetInfoMap,
                                 std::vector<MaterialType> const& materials,
                                 std::size_t totalMaterialCount) {
  ZoneScoped;

  MaterialPathTable materialPathTable;
  for (auto& row : materialPathTable) {
    row.resize(totalMaterialCount);
  }

  for (std::size_t i = 0; i < materials.size(); ++i) {
    ZoneScopedN("Extract material");

    MaterialType const& mat = materials[i];
    asset::MaterialAssetInfo newMatAssetInfo;
    newMatAssetInfo.compressionMode = asset::CompressionMode::none;

    VertexContentInfo const vertInfo = getVertInfo(mat);

    switch (_materialType) {
    case core::MaterialType::unlit:
      extractUnlitMaterialData(mat, newMatAssetInfo, textureAssetInfoMap);
      break;
    case core::MaterialType::lit: {
      extractLitMaterialData(mat, newMatAssetInfo, textureAssetInfoMap);
      break;
    }
    case core::MaterialType::pbr: {
      if (vertInfo.hasNormal && vertInfo.hasTangent && vertInfo.hasUV) {
        extractPbrOrFallbackMaterialData(mat, newMatAssetInfo,
                                         textureAssetInfoMap);
      } else {
        // fallback
        OBS_LOG_WARN("Missing vertex attributes - Pbr pipeline requires "
                     "normals, tangents and UV vertex attributes. Falling back "
                     "to lit material pipeline. Material "
                     "name: " +
                     getMaterialShortName(mat) + ". ");
        extractLitMaterialData(mat, newMatAssetInfo, textureAssetInfoMap);
      }
      break;
    }
    }

    newMatAssetInfo.transparent = isMaterialTransparent(mat);
    newMatAssetInfo.shaderPath =
        shaderPicker(mat, newMatAssetInfo.materialType);

    asset::Asset matAsset;
    if (asset::packMaterial(newMatAssetInfo, {}, matAsset)) {
      fs::path materialPath = projectPath / getMaterialName(mat, vertInfo);
      materialPath.replace_extension(".obsmat");

      if (asset::saveToFile(materialPath, matAsset)) {
        std::size_t vertInfoInteger = representAsInteger(vertInfo);
        int matInd = getMatInd(mat);
        materialPathTable[vertInfoInteger][matInd] =
            fs::relative(materialPath, projectPath);
      }
    }
  }

  return materialPathTable;
}

} /*namespace obsidian::asset_converter*/
