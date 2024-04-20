cmake_minimum_required(VERSION 3.24)

add_executable(ObsidianEditor
    "src/main.cpp"
    "src/editor_windows.cpp"
    "src/settings.cpp"
    "include/obsidian/editor/editor_windows.hpp"
    "include/obsidian/editor/settings.hpp"
    "include/obsidian/editor/item_list_data_source.hpp"
)

target_include_directories(ObsidianEditor
    PRIVATE
        "include"
)

target_link_libraries(ObsidianEditor
    PRIVATE
        AssetConverter
        Asset
        Project
        ObsidianEngine
        SDLWrapper
        DearImgui
        ImGuiFileDialog
        TracyClient
        Core
        Globals
        Platform
        nlohmann_json::nlohmann_json
)

get_target_property(EDITOR_OUTPUT_DIR ObsidianEditor BINARY_DIR)

add_custom_target(CopyAssetsToEditorDir
${CMAKE_COMMAND} -E copy_directory ${GENERATED_ASSETS_DIR} ${EDITOR_OUTPUT_DIR}/assets
)

add_dependencies(CopyAssetsToEditorDir GeneratedAssets)
add_dependencies(ObsidianEditor CopyAssetsToEditorDir)
