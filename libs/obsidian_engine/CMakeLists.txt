cmake_minimum_required(VERSION 3.24)

add_library(ObsidianEngine
    "src/obsidian_engine.cpp"
    "include/obsidian/obsidian_engine/obsidian_engine.hpp"
)

target_include_directories(ObsidianEngine
    PUBLIC
        "include"
)

target_link_libraries(ObsidianEngine
    PUBLIC
        VulkanRHI
        Input
        Window
        Scene
        RuntimeResource
        Project
        TracyClient
)
