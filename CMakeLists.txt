cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(apps)
add_subdirectory(libs)
add_subdirectory(third_party)
