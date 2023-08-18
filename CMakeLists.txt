cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(third_party)
add_subdirectory(libs)

add_executable(ObsidianEngine
    "src/main.cpp"
)

target_link_libraries(ObsidianEngine
    PRIVATE
        RHI
)
