cmake_minimum_required(VERSION 3.24)

add_library(RHI INTERFACE
    "include/obsidian/rhi/rhi.hpp"
)

target_include_directories(RHI
    INTERFACE
        include
)
