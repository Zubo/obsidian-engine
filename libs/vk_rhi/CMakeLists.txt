cmake_minimum_required(VERSION 3.24)

option (VULKAN_FORCE_VALIDATION_LAYERS "Force validation layers in non-debug builds" OFF)
option (VULKAN_DEBUG_FEATURES "Enable Vulkan debug features" OFF)

add_library(VulkanRHI
    "src/vk_rhi.cpp"
    "src/vk_rhi_draw.cpp"
    "src/vk_rhi_init.cpp"
    "src/vk_rhi_cleanup.cpp"
    "src/vk_initializers.cpp"
    "src/vk_pipeline_builder.cpp"
    "src/vk_render_pass_builder.cpp"
    "src/vk_mesh.cpp"
    "src/vk_descriptors.cpp"
    "src/vk_types.cpp"
    "src/vk_framebuffer.cpp"
    "src/vk_rhi_environment_maps.cpp"
    "src/vk_debug.cpp"
    "include/obsidian/vk_rhi/vk_rhi.hpp"
    "include/obsidian/vk_rhi/vk_check.hpp"
    "include/obsidian/vk_rhi/vk_types.hpp"
    "include/obsidian/vk_rhi/vk_initializers.hpp"
    "include/obsidian/vk_rhi/vk_mesh.hpp"
    "include/obsidian/vk_rhi/vk_pipeline_builder.hpp"
    "include/obsidian/vk_rhi/vk_render_pass_builder.hpp"
    "include/obsidian/vk_rhi/vk_deletion_queue.hpp"
    "include/obsidian/vk_rhi/vk_descriptors.hpp"
    "include/obsidian/vk_rhi/vk_framebuffer.hpp"
    "include/obsidian/vk_rhi/vk_frame_data.hpp"
    "include/obsidian/vk_rhi/vk_debug.hpp"
)

target_include_directories(VulkanRHI
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(VulkanRHI
    PUBLIC
        VulkanSDK
        RHI
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
    PRIVATE
        Core
        HashLibrary
        Renderdoc
        ThirdPartyImpl
        TracyClient
)

add_dependencies(VulkanRHI ShaderAssets)

if (VULKAN_FORCE_VALIDATION_LAYERS OR (DEFINED CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug"))
    target_compile_definitions(VulkanRHI
        PRIVATE
            USE_VULKAN_VALIDATION_LAYERS
    )
endif()

if (VULKAN_DEBUG_FEATURES)
    target_compile_definitions(VulkanRHI
        PRIVATE
            USE_VULKAN_DEBUG_FEATURES
    )
endif()
