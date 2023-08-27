cmake_minimum_required(VERSION 3.24)

add_library(Core
    "include/obsidian/core/keycode.hpp"
    "include/obsidian/core/logging.hpp"
    "include/obsidian/core/texture_format.hpp"
    "include/obsidian/core/material.hpp"
    "include/obsidian/core/vertex_type.hpp"
    "src/texture_format.cpp"
)

target_include_directories(Core
    PUBLIC
        "include"
)

target_link_libraries(Core
    PUBLIC
        glm
)
