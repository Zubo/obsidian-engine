cmake_minimum_required(VERSION 3.24)

option (ENABLE_RENDERDOC "Enable renderdoc api for GPU debugging" OFF)

add_library(Renderdoc
    "src/renderdoc.cpp"
    "include/renderdoc/renderdoc.hpp"
)

target_include_directories(Renderdoc
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if (${ENABLE_RENDERDOC})
    find_library(find_renderdoc NAMES renderdoc NO_CACHE)

    if (NOT ${find_renderdoc} STREQUAL "find_renderdoc-NOTFOUND")
        target_compile_definitions(ObsidianEngine
            PRIVATE
                RENDERDOC_ENABLED
                RENDERDOC_PATH="${find_renderdoc}"
        )
        message(STATUS "Renderdoc library available at path ${find_renderdoc}.")
    else()
        message(STATUS "Renderdoc library not available.")
    endif()
endif()
