cmake_minimum_required(VERSION 3.24)

add_library(Globals INTERFACE
    "include/obsidian/globals/file_extensions.hpp"
)

target_include_directories(Globals
    INTERFACE
        "include"
)
