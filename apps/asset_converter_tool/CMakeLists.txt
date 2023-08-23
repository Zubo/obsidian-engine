cmake_minimum_required(VERSION 3.24)

add_executable(ObsidianAssetConverterTool
    "src/main.cpp"
)

target_link_libraries(ObsidianAssetConverterTool
    PRIVATE
        AssetConverter
        Core
)
