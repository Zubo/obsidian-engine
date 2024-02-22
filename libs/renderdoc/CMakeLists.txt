cmake_minimum_required(VERSION 3.24)

option (ENABLE_RENDERDOC "Enable renderdoc api for GPU debugging" OFF)

add_library(Renderdoc
    "src/renderdoc.cpp"
    "include/obsidian/renderdoc/renderdoc.hpp"
)

target_include_directories(Renderdoc
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if (${ENABLE_RENDERDOC})
    if (WIN32)
        find_path(RENDERDOC_INCLUDE_DIR 
            NAMES renderdoc_app.h
            PATHS 
                "C:/Program Files/RenderDoc"
                "C:/Program Files (x86)/RenderDoc"
            REQUIRED
        )

        add_library(RenderdocInclude INTERFACE)
        target_include_directories(RenderdocInclude INTERFACE ${RENDERDOC_INCLUDE_DIR})
        target_link_libraries(Renderdoc
            PRIVATE
                RenderdocInclude
        )

        find_path(renderdoc_path 
            NAMES renderdoc.dll
            HINTS "$ENV{RENDERDOC_PATH}"
            PATHS 
                "C:/Program Files/RenderDoc"
                "C:/Program Files (x86)/RenderDoc"
            REQUIRED
        )

    else()
        find_library(renderdoc_path NAMES renderdoc NO_CACHE)
    endif()

    if (NOT ${renderdoc_path} STREQUAL "renderdoc_path-NOTFOUND")
        target_compile_definitions(Renderdoc
            PRIVATE
                RENDERDOC_ENABLED
                RENDERDOC_PATH="${renderdoc_path}"
        )
        message(STATUS "Renderdoc library available at path ${renderdoc_path}.")
    else()
        message(STATUS "Renderdoc library not available.")
    endif()
endif()
