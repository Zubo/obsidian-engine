cmake_minimum_required(VERSION 3.24)

option (FORCE_VALIDATION_LAYERS "Force validation layers in non-debug builds" OFF)

add_library(RHIVulkan
    "src/vk_engine.cpp"
    "src/vk_engine_input_handling.cpp"
    "src/vk_engine_draw.cpp"
    "src/vk_engine_init.cpp"
    "src/vk_engine_cleanup.cpp"
    "src/vk_initializers.cpp"
    "src/vk_pipeline_builder.cpp"
    "src/vk_mesh.cpp"
    "src/vk_descriptors.cpp"
    "include/rhi_vk/vk_engine.hpp"
    "include/rhi_vk/vk_check.hpp"
    "include/rhi_vk/vk_types.hpp"
    "include/rhi_vk/vk_initializers.hpp"
    "include/rhi_vk/vk_mesh.hpp"
    "include/rhi_vk/vk_pipeline_builder.hpp"
    "include/rhi_vk/vk_deletion_queue.hpp"
    "include/rhi_vk/vk_descriptors.hpp"
)

target_include_directories(RHIVulkan
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(RHIVulkan
    PUBLIC
        VulkanSDK
        SDLInterface
        vk-bootstrap::vk-bootstrap
        VulkanMemoryAllocator
        glm
        tinyobjloader
        TracyClient
        HashLibrary
        Renderdoc
        ThirdPartyImpl
)

if (${FORCE_VALIDATION_LAYERS} OR ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_definitions(RHIVulkan
        PRIVATE
            USE_VULKAN_VALIDATION_LAYERS
    )
endif()
