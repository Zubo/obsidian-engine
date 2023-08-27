cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(GENERATED_ASSETS_DIR "${CMAKE_BINARY_DIR}/assets")
add_custom_target(GeneratedAssets)

add_subdirectory(apps)
add_subdirectory(libs)
add_subdirectory(third_party)
add_subdirectory(shaders)
