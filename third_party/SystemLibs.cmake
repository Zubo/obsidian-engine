include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("stb/stb_image.h" stb_image_exists)

if (NOT stb_image_exists)
    message(FATAL_ERROR "Include file stb/stb_image.h is missing. Please install the stb library.")
endif()
