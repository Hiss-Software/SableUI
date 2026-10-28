cmake_minimum_required(VERSION 3.20)
project(sableui VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SABLEUI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(SABLEUI_WARNINGS_AS_ERRORS)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

# Version ----------------------------------------------------------------------

# The library version is derived from the latest git tag (see
# cmake/gen_version.cmake). The project() version above is only a fallback
# for builds from a source archive without a .git directory. The header is
# regenerated on every build, so re-tagging is enough to bump the version.
set(SABLEUI_VERSION_HEADER ${CMAKE_BINARY_DIR}/sableui_version.h)
add_custom_target(sableui_version ALL
    COMMAND ${CMAKE_COMMAND}
        -DGIT_DIR=${CMAKE_SOURCE_DIR}
        -DOUT_FILE=${SABLEUI_VERSION_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/gen_version.cmake
    BYPRODUCTS ${SABLEUI_VERSION_HEADER}
    VERBATIM)

# Core library -----------------------------------------------------------------

add_library(sableui SHARED
    src/sableui_api.cpp)

add_dependencies(sableui sableui_version)

target_include_directories(sableui
    PRIVATE ${CMAKE_BINARY_DIR}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_definitions(sableui PRIVATE SABLEUI_BUILDING)
target_compile_definitions(sableui PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# Common Windows libraries.
target_link_libraries(sableui PRIVATE d2d1 dwrite dwmapi ole32 user32 gdi32 imm32)

set_target_properties(sableui PROPERTIES OUTPUT_NAME "sableui")

if(MSVC)
    # MSVC: warnings, UTF-8 source encoding, multi-threaded runtime.
    target_compile_options(sableui PRIVATE
        /W4
        /permissive-
        /utf-8
        /Zc:__cplusplus
        /EHsc)
    target_compile_definitions(sableui PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # GCC / Clang (MinGW-w64).
    target_compile_options(sableui PRIVATE
        -Wall -Wextra -Wpedantic
        -fms-extensions
        -Wno-cast-function-type)
    target_link_options(sableui PRIVATE
        -static-libstdc++
        -static-libgcc
        -static)
endif()

# Installation -----------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS sableui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES include/sableui/sableui.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sableui)