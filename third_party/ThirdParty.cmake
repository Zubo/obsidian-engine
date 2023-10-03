
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

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

FetchContent_Declare(fetch_hash_library
    GIT_REPOSITORY https://github.com/stbrumme/hash-library.git
    GIT_TAG hash_library_v8
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fetch_hash_library)

add_library(HashLibrary
    ${fetch_hash_library_SOURCE_DIR}/crc32.cpp
)

target_include_directories(HashLibrary
    PUBLIC
     ${fetch_hash_library_SOURCE_DIR}
)

FetchContent_Declare(fetch_dear_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.8-docking
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fetch_dear_imgui)

add_library(DearImgui STATIC
    ${fetch_dear_imgui_SOURCE_DIR}/imgui.h
    ${fetch_dear_imgui_SOURCE_DIR}/imgui.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/imgui_draw.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/imgui_demo.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/imgui_tables.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    ${fetch_dear_imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)

target_include_directories(DearImgui
    PUBLIC
        ${fetch_dear_imgui_SOURCE_DIR}
)

target_link_libraries(DearImgui
    PUBLIC
        ${Vulkan_LIBRARIES}
        ${SDL2_LIBRARIES}
)


FetchContent_Declare(fetch_lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.9.4
    SOURCE_SUBDIR ./build/cmake
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fetch_lz4)

FetchContent_Declare(fetch_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    GIT_PROGRESS TRUE
)

FetchContent_Declare(fetch_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_PROGRESS TRUE
)

FetchContent_Populate(fetch_stb)

set(STB_INCLUDE_DIR ${fetch_stb_BINARY_DIR})
file(COPY ${fetch_stb_SOURCE_DIR}/stb_image.h DESTINATION ${STB_INCLUDE_DIR}/stb)

add_library(StbImage
    INTERFACE
        ${STB_INCLUDE_DIR}/stb/stb_image.h
)

target_include_directories(StbImage
    INTERFACE
        ${STB_INCLUDE_DIR}/
)

FetchContent_MakeAvailable(fetch_json)
