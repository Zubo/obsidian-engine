cmake_minimum_required(VERSION 3.24)

add_executable(ObsidianEditor
    "src/main.cpp"
    "src/editor_windows.cpp"
    "include/obsidian/editor/editor_windows.hpp"
)

target_include_directories(ObsidianEditor
    PRIVATE
        "include"
)

target_link_libraries(ObsidianEditor
    PRIVATE
        AssetConverter
        ObsidianEngine
        SDLWrapper
        DearImgui
        TracyClient
        Core
)
