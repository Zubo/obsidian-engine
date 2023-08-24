cmake_minimum_required(VERSION 3.24)

add_library(Core INTERFACE
    "include/core/keycode.hpp"
    "include/core/logging.hpp"
)

target_include_directories(Core
    INTERFACE
        "include"
)

target_link_libraries(Core
    INTERFACE
        glm
)
