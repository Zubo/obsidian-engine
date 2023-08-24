cmake_minimum_required(VERSION 3.24)

add_library(Project
    "src/project.cpp"
    "include/obsidian/project/project.hpp"
)

target_include_directories(Project
    PUBLIC
        "include"
)

target_link_libraries(Project
    PRIVATE
        Core
)
