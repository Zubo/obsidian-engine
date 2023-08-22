cmake_minimum_required(VERSION 3.24)

include(ThirdParty.cmake)
include (SystemLibs.cmake)

add_library(VulkanSDK INTERFACE)

target_link_libraries(VulkanSDK
    INTERFACE
        ${Vulkan_LIBRARIES}
)

target_include_directories(VulkanSDK
    INTERFACE
        ${VULKAN_INCLUDE_DIRS}
)

add_subdirectory(single_header_impl)
