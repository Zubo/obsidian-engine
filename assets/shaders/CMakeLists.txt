cmake_minimum_required(VERSION 3.24)

set(SHADER_SRC_FILES
    "built-in/default-unlit.frag"
    "built-in/default-unlit.vert"
    "built-in/default.frag"
    "built-in/default.vert"
    "built-in/shadow-pass.vert"
    "built-in/shadow-pass.frag"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

set(SHADER_INPUT_LIST)
set(SHADER_OUTPUT_LIST)

foreach(SHADER_SRC ${SHADER_SRC_FILES})
    list(APPEND
        SHADER_INPUT_LIST
            "src/${SHADER_SRC}"
    )
    string(REPLACE "." "-" SHADER_OUTPUT_FILE_NAME ${SHADER_SRC})
    list(APPEND
        SHADER_OUTPUT_LIST
            "${SHADER_OUTPUT_DIR}/${SHADER_OUTPUT_FILE_NAME}.spv"
            "${SHADER_OUTPUT_DIR}/${SHADER_OUTPUT_FILE_NAME}-dbg.spv"
    )
endforeach()

add_custom_target(ShadersSPIRV ALL
    python3 ${PROJECT_SOURCE_DIR}/scripts/compile_shaders.py -o ${CMAKE_BINARY_DIR}/shaders
    DEPENDS ${SHADER_INPUT_LIST}
    BYPRODUCTS ${SHADER_OUTPUT_LIST}
)

add_custom_target(ShaderAssets ALL
    ObsidianAssetConverterTool "-s" "${CMAKE_BINARY_DIR}/shaders/built-in" "-d" "${GENERATED_ASSETS_DIR}/obsidian/shaders"
    DEPENDS ${SHADER_OUTPUT_LIST}
)

add_dependencies(ShaderAssets ObsidianAssetConverterTool ShadersSPIRV)
add_dependencies(GeneratedAssets ShaderAssets)
