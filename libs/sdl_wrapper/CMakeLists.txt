cmake_minimum_required(VERSION 3.24)

add_library(SDLWrapper
    "src/sdl_backend.cpp"
    "src/sdl_window_backend.cpp"
    "include/sdl_wrapper/sdl_backend.hpp"
    "include/sdl_wrapper/sdl_window_backend.hpp"
)

target_include_directories(SDLWrapper
    PUBLIC
        "include"
    PRIVATE
        ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(SDLWrapper
    PUBLIC
        Window
        Core
        ${SDL2_LIBRARIES}
)
