cmake_minimum_required(VERSION 3.24)

add_library(Task
        "src/task_executor.cpp"
        "src/task.cpp"
        "include/obsidian/task/task_executor.hpp"
        "include/obsidian/task/task_type.hpp"
        "include/obsidian/task/task.hpp"
)

target_include_directories(Task
    PUBLIC
        "include"
)

target_link_libraries(Task
    PUBLIC
        Core
)

add_executable(TestTask
    "test/test_task.cpp"
)

target_link_libraries(TestTask
    PRIVATE
        Task
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(TestTask)
