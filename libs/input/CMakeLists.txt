cmake_minimum_required(VERSION 3.24)

add_library(Input
    "include/obsidian/input/input_context.hpp"
    "include/obsidian/input/key_input_emitter.hpp"
    "include/obsidian/input/mouse_motion_emitter.hpp"
    "include/obsidian/input/window_event_emitter.hpp"
    "src/key_input_emitter.cpp"
    "src/mouse_motion_emitter.cpp"
    "src/window_event_emitter.cpp"
)

target_include_directories(Input
    PUBLIC
        "include"
)

target_link_libraries(Input
    PUBLIC
        Core
)
