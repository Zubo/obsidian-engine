cmake_minimum_required(VERSION 3.24)

add_library(task
        "src/task_executor.cpp"
        "src/flow_builder.cpp"
        "src/task.cpp"
        "include/obsidian/task/task_executor.hpp"
        "include/obsidian/task/flow.hpp"
        "include/obsidian/task/flow_builder.hpp"
        "include/obsidian/task/task_type.hpp"
        "include/obsidian/task/task.hpp"
)

target_include_directories(task
    PUBLIC
        "include"
)

add_executable(test_task
    "test/test_task_executor.cpp"
)

target_link_libraries(test_task
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(test_task)
