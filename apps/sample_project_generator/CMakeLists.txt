cmake_minimum_required(VERSION 3.24)

add_executable(ObsidianSampleProjectGenerator
    "src/main.cpp"
)

target_link_libraries(ObsidianSampleProjectGenerator
    PRIVATE
        Project
        Core
)

target_compile_definitions(
    ObsidianSampleProjectGenerator
        PRIVATE
            SAMPLE_ASSETS_DIR="${SAMPLE_ASSETS_DIR}"
)

add_dependencies(ObsidianSampleProjectGenerator GeneratedAssets)
