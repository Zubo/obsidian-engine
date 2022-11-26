cmake_minimum_required(VERSION 3.24)

project(VkGuide
    LANGUAGES CXX)


add_executable(vkguide
    "src/main.cpp"
)

target_include_directories(vkguide PRIVATE include)

target_link_libraries(vkguide vulkan)