cmake_minimum_required(VERSION 3.24)

add_library(Asset
    "src/asset_io.cpp"
    "include/asset/asset.hpp"
    "include/asset/asset_io.hpp"
)

target_include_directories(Asset
    PUBLIC
        "include"
)
