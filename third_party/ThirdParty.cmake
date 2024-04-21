
find_package(Vulkan REQUIRED)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    fetch_sdl
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG SDL2
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_sdl)

FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG v0.7
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_vk_bootstrap)

FetchContent_Declare(
    fetch_vulkan_memory_allocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG 0e89587db3ebee4d463f191bd296374c5fafc8ea
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_vulkan_memory_allocator)

FetchContent_Declare(fetch_glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_glm)

FetchContent_Declare(fetch_tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc10
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_tinyobjloader)

set(TINYGLTF_HEADER_ONLY ON)
FetchContent_Declare(fetch_tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG release
    GIT_PROGRESS TRUE
    SYSTEM

)

FetchContent_MakeAvailable(fetch_tinygltf)

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
    SYSTEM
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
    SYSTEM
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
        SDL2::SDL2-static
)


FetchContent_Declare(fetch_lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.9.4
    SOURCE_SUBDIR ./build/cmake
    GIT_PROGRESS TRUE
    SYSTEM
)

set(LZ4_BUILD_CLI OFF CACHE BOOL "Build lz4 program" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "Build lz4c program with legacy argument support" FORCE)

FetchContent_MakeAvailable(fetch_lz4)

FetchContent_Declare(fetch_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_Declare(fetch_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_PROGRESS TRUE
    SYSTEM
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

FetchContent_Declare(fetch_gtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_gtest)

FetchContent_Declare(fetch_imguifiledialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog
    GIT_TAG v0.6.7
    GIT_PROGRESS TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fetch_imguifiledialog)

FetchContent_Declare(fetch_deccer_cubes
    GIT_REPOSITORY https://github.com/GraphicsProgramming/deccer-cubes.git
    GIT_TAG main
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fetch_deccer_cubes)

set(SAMPLE_ASSETS_DIR ${fetch_deccer_cubes_SOURCE_DIR} CACHE PATH "Path to directory containing assets for generating test scene." FORCE)

target_link_libraries(ImGuiFileDialog PUBLIC DearImgui)

if (WIN32)
    set_target_properties(
        gtest
        gtest_main
        gmock
        gmock_main
        SDL2
        SDL2_test
        SDL2main
        SDL2-static
        sdl_headers_copy
        TracyClient
        tinyobjloader
        vk-bootstrap
        lz4_static
        loader_example
        DearImgui
        ImGuiFileDialog
        uninstall
        VulkanMemoryAllocator
        StbImage
        HashLibrary
            PROPERTIES
                FOLDER ThirdParty
    )
endif()
