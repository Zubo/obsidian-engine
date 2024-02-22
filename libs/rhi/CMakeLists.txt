cmake_minimum_required(VERSION 3.24)

add_library(RHI
    "include/obsidian/rhi/rhi.hpp"
    "include/obsidian/rhi/resource_rhi.hpp"
    "include/obsidian/rhi/submit_types_rhi.hpp"
    "src/resource_rhi.cpp"
)

target_include_directories(RHI
    PUBLIC
        include
)

target_link_libraries(RHI
    PUBLIC
        Core
    INTERFACE
        Task
        glm
)
