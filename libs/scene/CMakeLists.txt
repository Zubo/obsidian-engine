cmake_minimum_required(VERSION 3.24)

add_library(Scene
    "include/scene/scene.hpp"
    "include/scene/camera.hpp"
    "include/scene/game_object.hpp"
    "src/scene.cpp"
    "src/camera.cpp"
    "src/game_object.cpp"
)

target_include_directories(Scene
    PUBLIC
        "include"
)

target_link_libraries(Scene
    PRIVATE
        Input
        Core
)
