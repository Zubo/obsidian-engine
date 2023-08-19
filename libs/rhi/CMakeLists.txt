cmake_minimum_required(VERSION 3.24)

add_library(RHI
    "src/rhi.cpp"
    "include/rhi/rhi.hpp"
)

target_include_directories(RHI
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(RHI
    PRIVATE
        VulkanRHI
)
