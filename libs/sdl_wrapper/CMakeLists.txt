cmake_minimum_required(VERSION 3.24)

add_library(SDLWrapper
    "src/sdl_backend.cpp"
    "src/sdl_window_backend.cpp"
    "include/obsidian/sdl_wrapper/sdl_backend.hpp"
    "include/obsidian/sdl_wrapper/sdl_window_backend.hpp"
)

target_include_directories(SDLWrapper
    PUBLIC
        "include"
)

target_link_libraries(SDLWrapper
    PUBLIC
        Window
        Core
        RHI
        SDL2::SDL2
    PRIVATE
        VulkanRHI
)
