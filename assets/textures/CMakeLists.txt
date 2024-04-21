cmake_minimum_required(VERSION 3.24)

set(INPUT_TEXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/files")

set(INPUT_TEXTURE_FILE_NAMES
    "default-texture.png"
)

set(INPUT_TEXTURE_PATHS)
set(OUTPUT_TEXTURE_PATHS)
set(OUTPUT_TEXTURE_DIR "${STANDARD_ASSETS_DIR}/obsidian/textures")

foreach (TEX_FILE_NAME ${INPUT_TEXTURE_FILE_NAMES})
    list(APPEND
        INPUT_TEXTURE_PATHS
            ${INPUT_TEXTURE_DIR}/${TEX_FILE_NAME}
    )
    string(REPLACE ".png" ".obstex" OUT_TEX_FILE_NAME ${TEX_FILE_NAME})
    list(APPEND
        OUTPUT_TEXTURE_PATHS
            ${OUTPUT_TEXTURE_DIR}/${OUT_TEX_FILE_NAME}
    )
endforeach()

add_custom_target(TextureAssets ALL
    ${CMAKE_COMMAND} -E remove_directory ${OUTPUT_TEXTURE_DIR}
    COMMAND ObsidianAssetConverterTool "-s" "${INPUT_TEXTURE_DIR}" "-d" "${OUTPUT_TEXTURE_DIR}"
    DEPENDS ${INPUT_TEXTURE_PATHS}
    BYPRODUCTS ${OUTPUT_TEXTURE_PATHS}
)

add_dependencies(TextureAssets ObsidianAssetConverterTool)
add_dependencies(GeneratedAssets TextureAssets)
