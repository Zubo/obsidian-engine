cmake_minimum_required(VERSION 3.24)

project(ObsidianEngine
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

enable_testing()

option(ENABLE_ADDRESS_SANITIZER "Enables address sanitizer for the whole project." OFF)

if (ENABLE_ADDRESS_SANITIZER)
    if(MSVC)
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

set(GENERATED_ASSETS_DIR "${CMAKE_BINARY_DIR}/generated-assets" CACHE PATH "GENERATED_ASSETS_DIR")
configure_file("${CMAKE_SOURCE_DIR}/.clang-format" "${CMAKE_BINARY_DIR}/.clang-format" COPYONLY)

add_subdirectory(apps)
add_subdirectory(libs)
add_subdirectory(third_party)
add_subdirectory(assets)
