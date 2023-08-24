cmake_minimum_required(VERSION 3.24)

add_library(Window
    "src/window.cpp"
    "include/obsidian/window/window.hpp"
    "include/obsidian/window/window_impl_interface.hpp"
    "include/obsidian/window/window_events.hpp"
)

target_include_directories(Window
    PUBLIC
        "include"
)

target_link_libraries(Window
    PUBLIC
        ${Vulkan_LIBRARIES}
        Core
    PRIVATE
        Input
)
