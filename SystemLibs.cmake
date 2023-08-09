if (${ENABLE_RENDERDOC})
    find_library(find_renderdoc NAMES renderdoc NO_CACHE)

    if (NOT ${find_renderdoc} STREQUAL "find_renderdoc-NOTFOUND")
        target_compile_definitions(ObsidianEngine
            PRIVATE
                RENDERDOC_ENABLED
                RENDERDOC_PATH="${find_renderdoc}"
        )
        message(STATUS "Renderdoc library available at path ${find_renderdoc}.")
    else()
        message(STATUS "Renderdoc library not available.")
    endif()
endif()

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("stb/stb_image.h" stb_image_exists)

if (NOT stb_image_exists)
    message(FATAL_ERROR "Include file stb/stb_image.h is missing. Please install the stb library.")
endif()
