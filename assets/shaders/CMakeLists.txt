cmake_minimum_required(VERSION 3.24)

set (SHADER_SRC_DIR "built-in")

set(SHADER_SRC_FILES
    "default-unlit.frag"
    "default-unlit.vert"
    "default.frag"
    "default.vert"
    "default-pbr.frag"
    "default-pbr.vert"
    "depth-only.vert"
    "depth-only.frag"
    "ssao.vert"
    "ssao.frag"
    "post-processing.vert"
    "post-processing.frag"
    "water.vert"
    "water.frag"
    "include/blend.glsl"
    "include/blinn-phong-lighting.glsl"
    "include/camera.glsl"
    "include/environment-maps.glsl"
    "include/global-settings.glsl"
    "include/lighting.glsl"
    "include/lit-material.glsl"
    "include/pbr-lighting.glsl"
    "include/pbr-material.glsl"
    "include/renderpass-data.glsl"
    "include/ssao.glsl"
    "include/timer.glsl"
    "include/unlit-material.glsl"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

set(SHADER_INPUT_LIST)
set(SHADER_OUTPUT_LIST)

foreach(SHADER_SRC ${SHADER_SRC_FILES})
    if (NOT ${SHADER_SRC} MATCHES "^include/")
        list(APPEND
            SHADER_INPUT_LIST
                "src/${SHADER_SRC_DIR}/${SHADER_SRC}"
        )

        string(REPLACE "." "-" SHADER_OUTPUT_FILE_NAME ${SHADER_SRC})
        list(APPEND
            SHADER_OUTPUT_LIST
                "${SHADER_OUTPUT_DIR}/${SHADER_SRC_DIR}/${SHADER_OUTPUT_FILE_NAME}.spv"
        )

        if (${SHADER_OUTPUT_FILE_NAME} MATCHES "(^default)(-unlit)?((-frag)|(-vert))")
            list(APPEND
                SHADER_OUTPUT_LIST
                    "${SHADER_OUTPUT_DIR}/${SHADER_SRC_DIR}/c-${SHADER_OUTPUT_FILE_NAME}.spv"
                    "${SHADER_OUTPUT_DIR}/${SHADER_SRC_DIR}/u-${SHADER_OUTPUT_FILE_NAME}.spv"
                    "${SHADER_OUTPUT_DIR}/${SHADER_SRC_DIR}/cu-${SHADER_OUTPUT_FILE_NAME}.spv"
            )
        endif()
    endif()
endforeach()

option(DEBUG_SHADERS "Prevent shader optimization and add shader source metadata to the compiled spirv." OFF)

if(DEBUG_SHADERS)
    set(SHADERS_DEBUG "--dbg")
endif()

add_custom_command(
    OUTPUT ${SHADER_OUTPUT_LIST}
    COMMAND ${CMAKE_COMMAND} -E remove file ${SHADER_OUTPUT_LIST}
    COMMAND python ${PROJECT_SOURCE_DIR}/scripts/compile_shaders.py -o ${CMAKE_BINARY_DIR}/shaders ${SHADERS_DEBUG}
    DEPENDS ${SHADER_INPUT_LIST}
)

add_custom_target(ShaderAssets ALL
    ObsidianAssetConverterTool "-s" "${CMAKE_BINARY_DIR}/shaders/built-in" "-d" "${GENERATED_ASSETS_DIR}/obsidian/shaders"
    DEPENDS ${SHADER_OUTPUT_LIST}
)

add_dependencies(ShaderAssets ObsidianAssetConverterTool)
add_dependencies(GeneratedAssets ShaderAssets)
