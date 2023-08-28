cmake_minimum_required(VERSION 3.24)

add_custom_target(TextureAssets ALL
    ObsidianAssetConverterTool "-s" "${CMAKE_CURRENT_SOURCE_DIR}/files" "-d" "${GENERATED_ASSETS_DIR}/obsidian/textures"
)

add_dependencies(GeneratedAssets TextureAssets)
