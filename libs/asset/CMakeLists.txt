cmake_minimum_required(VERSION 3.24)

add_library(Asset
    "src/asset.cpp"
    "src/asset_io.cpp"
    "src/asset_info.cpp"
    "src/texture_asset_info.cpp"
    "src/mesh_asset_info.cpp"
    "src/shader_asset_info.cpp"
    "src/material_asset_info.cpp"
    "src/scene_asset_info.cpp"
    "src/utility.cpp"
    "include/obsidian/asset/asset.hpp"
    "include/obsidian/asset/asset_io.hpp"
    "include/obsidian/asset/asset_info.hpp"
    "include/obsidian/asset/texture_asset_info.hpp"
    "include/obsidian/asset/mesh_asset_info.hpp"
    "include/obsidian/asset/shader_asset_info.hpp"
    "include/obsidian/asset/material_asset_info.hpp"
    "include/obsidian/asset/scene_asset_info.hpp"
    "include/obsidian/asset/utility.hpp"
)

target_include_directories(Asset
    PUBLIC
        "include"
)

target_link_libraries(Asset
    PRIVATE
        Core
        nlohmann_json::nlohmann_json
        lz4_static
        TracyClient
)
