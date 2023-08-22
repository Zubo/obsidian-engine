cmake_minimum_required(VERSION 3.24)

add_library(Scene
    "include/scene/scene.hpp"
    "src/scene.cpp"
)

target_include_directories(Scene
    PUBLIC
        "include"
)

target_link_libraries(Scene
    PUBLIC
        glm
    PRIVATE
        Input
        Core
)
