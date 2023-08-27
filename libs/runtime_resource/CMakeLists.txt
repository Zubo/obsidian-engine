cmake_minimum_required(VERSION 3.24)

add_library(RuntimeResource
    "src/runtime_resource_manager.cpp"
    "src/runtime_resource.cpp"
    "include/obsidian/runtime_resource/runtime_resource_manager.hpp"
    "include/obsidian/runtime_resource/runtime_resource.hpp"
)

target_include_directories(RuntimeResource
    PUBLIC
        "include"
)

target_link_libraries(RuntimeResource
    PUBLIC
        Asset
        RHI
    PRIVATE
        VulkanRHI # To be removed when proper interface gets implemented in RHI
        Project
)
