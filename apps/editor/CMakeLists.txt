cmake_minimum_required(VERSION 3.24)

add_executable(ObsidianEditor
    "src/main.cpp"
)

target_include_directories(ObsidianEditor
    PRIVATE
        "include"
)

target_link_libraries(ObsidianEditor
    PRIVATE
        VulkanRHI
        DearImgui
        TracyClient
)
