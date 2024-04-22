cmake_minimum_required(VERSION 3.24)

add_library(Serialization
    "src/game_object_data_serialization.cpp"
    "src/scene_data_serialization.cpp"
    "include/obsidian/serialization/game_object_data_serialization.hpp"
    "include/obsidian/serialization/scene_data_serialization.hpp"
    "include/obsidian/serialization/serialization.hpp"
)

target_include_directories(Serialization
    PUBLIC
        "include"
)

target_link_libraries(Serialization
    PUBLIC
        Core
        glm
        nlohmann_json::nlohmann_json
)
