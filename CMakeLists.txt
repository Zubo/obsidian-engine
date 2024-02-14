cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

enable_testing()

set(GENERATED_ASSETS_DIR "${CMAKE_BINARY_DIR}/generated-assets" CACHE PATH "GENERATED_ASSETS_DIR")

add_subdirectory(apps)
add_subdirectory(libs)
add_subdirectory(third_party)
add_subdirectory(assets)
