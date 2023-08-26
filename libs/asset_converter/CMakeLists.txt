cmake_minimum_required(VERSION 3.24)

add_library(AssetConverter
    "src/asset_converter.cpp"
    "include/obsidian/asset_converter/asset_converter.hpp"
)

target_include_directories(AssetConverter
    PUBLIC
        "include"
)

target_link_libraries(AssetConverter
    PRIVATE
        Asset
        Core
        Globals
        ThirdPartyImpl
        tinyobjloader
)
