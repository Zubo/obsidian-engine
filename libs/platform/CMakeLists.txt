cmake_minimum_required(VERSION 3.24)

add_library(Platform
    "src/environment.cpp"
    "include/obsidian/platform/environment.hpp"
)

target_include_directories(Platform
    PUBLIC
        "include"
)
