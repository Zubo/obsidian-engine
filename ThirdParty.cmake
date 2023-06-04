
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG v0.7
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

FetchContent_Declare(
    fetch_vulkan_memory_allocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG 0e89587db3ebee4d463f191bd296374c5fafc8ea
)
FetchContent_MakeAvailable(fetch_vulkan_memory_allocator)

FetchContent_Declare(fetch_glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(fetch_glm)

FetchContent_Declare(fetch_tinyobjloader

    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc10
)
FetchContent_MakeAvailable(fetch_tinyobjloader)

option(TRACY_ENABLE "" OFF)

FetchContent_Declare(fetch_tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fetch_tracy)
