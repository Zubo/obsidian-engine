cmake_minimum_required(VERSION 3.24)

add_library(Core
    "include/obsidian/core/keycode.hpp"
    "include/obsidian/core/logging.hpp"
    "include/obsidian/core/texture_format.hpp"
    "include/obsidian/core/material.hpp"
    "include/obsidian/core/vertex_type.hpp"
    "include/obsidian/core/utils/visitor.hpp"
    "include/obsidian/core/utils/functions.hpp"
    "include/obsidian/core/utils/utils.hpp"
    "include/obsidian/core/utils/texture_utils.hpp"
    "include/obsidian/core/utils/aabb.hpp"
    "include/obsidian/core/shapes.hpp"
    "src/texture_format.cpp"
    "src/utils/texture_utils.cpp"
    "src/utils/aabb.cpp"
)

target_include_directories(Core
    PUBLIC
        "include"
)

target_link_libraries(Core
    PUBLIC
        glm
        TracyClient
)

add_executable(TestCore
    "test/test_texture_utils.cpp"
)

target_link_libraries(TestCore
    PRIVATE
        Core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(TestCore)

