cmake_minimum_required(VERSION 3.24)

project(VkGuide
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(vkguide
    "src/main.cpp"
    "src/vk_engine.cpp"
    "include/vk_init.hpp"
    "include/vk_engine.hpp"
    "include/vk_types.hpp"
)

target_include_directories(vkguide PRIVATE include)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(third-party/vk-bootstrap)

target_link_libraries(vkguide
    PRIVATE
    ${VULKAN_LIBRARIES}
    ${SDL2_LIBRARIES}
    vk-bootstrap
)

target_include_directories(vkguide PRIVATE
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)
