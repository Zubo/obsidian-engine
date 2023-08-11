cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

option (ENABLE_RENDERDOC "Enable renderdoc api for GPU debugging" OFF)
option (FORCE_VALIDATION_LAYERS "Force validation layers in non-debug builds" OFF)

add_executable(ObsidianEngine
    "src/main.cpp"
    "src/vk_engine.cpp"
    "src/vk_engine_input_handling.cpp"
    "src/vk_initializers.cpp"
    "src/vk_mesh.cpp"
    "src/renderdoc.cpp"
    "src/stbi_impl.cpp"
    "src/vma_impl.cpp"
    "include/vk_engine.hpp"
    "include/vk_types.hpp"
    "include/vk_initializers.hpp"
    "include/vk_mesh.hpp"
    "include/renderdoc.hpp"
)

target_include_directories(ObsidianEngine PRIVATE include)

if (${FORCE_VALIDATION_LAYERS} OR ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_definitions(ObsidianEngine
        PRIVATE
            USE_VULKAN_VALIDATION_LAYERS
    )
endif()

include(ThirdParty.cmake)

target_link_libraries(ObsidianEngine
    PRIVATE
        ${Vulkan_LIBRARIES}
        ${SDL2_LIBRARIES}
        vk-bootstrap::vk-bootstrap
        VulkanMemoryAllocator
        glm
        tinyobjloader
        TracyClient
)

include (SystemLibs.cmake)

target_include_directories(ObsidianEngine PRIVATE
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)
