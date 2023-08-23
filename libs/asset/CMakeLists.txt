cmake_minimum_required(VERSION 3.24)

add_library(Asset
    "src/asset_io.cpp"
    "src/asset_info.cpp"
    "src/texture_asset_info.cpp"
    "src/mesh_asset_info.cpp"
    "include/asset/asset.hpp"
    "include/asset/asset_io.hpp"
    "include/asset/asset_info.hpp"
    "include/asset/texture_asset_info.hpp"
    "include/asset/mesh_asset_info.hpp"
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
)
