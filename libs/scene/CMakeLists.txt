cmake_minimum_required(VERSION 3.24)

add_library(Scene
    "include/obsidian/scene/scene.hpp"
    "include/obsidian/scene/camera.hpp"
    "include/obsidian/scene/game_object.hpp"
    "src/scene.cpp"
    "src/camera.cpp"
    "src/game_object.cpp"
)

target_include_directories(Scene
    PUBLIC
        "include"
)

target_link_libraries(Scene
    PUBLIC
        RuntimeResource
    PRIVATE
        Input
        Core
)
