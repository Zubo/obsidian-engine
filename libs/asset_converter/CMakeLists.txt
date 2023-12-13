cmake_minimum_required(VERSION 3.24)

add_library(AssetConverter
    "src/asset_converter.cpp"
    "src/asset_converter_helpers.cpp"
    "include/obsidian/asset_converter/asset_converter.hpp"
    "include/obsidian/asset_converter/asset_converter_helpers.hpp"
    "include/obsidian/asset_converter/vertex_content_info.hpp"
)

target_include_directories(AssetConverter
    PUBLIC
        "include"
)

target_link_libraries(AssetConverter
    PUBLIC
        Asset
        Core
    PRIVATE
        Task
        Globals
        ThirdPartyImpl
        tinyobjloader
        tinygltf
        StbImage
        TracyClient
        Serialization
        nlohmann_json::nlohmann_json
)
