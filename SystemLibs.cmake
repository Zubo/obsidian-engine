if (${ENABLE_RENDERDOC})
    find_library(find_renderdoc NAMES renderdoc NO_CACHE)

    if (NOT ${find_renderdoc} STREQUAL "find_renderdoc-NOTFOUND")
        target_compile_definitions(ObsidianEngine
            PRIVATE
                RENDERDOC_ENABLED
                RENDERDOC_PATH="${find_renderdoc}"
        )
        message(STATUS "Renderdoc library available at path ${}.")
    else()
        message(STATUS "Renderdoc library not available.")
    endif()
endif()
