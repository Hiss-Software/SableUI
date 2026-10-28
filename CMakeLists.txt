cmake_minimum_required(VERSION 3.20)
project(sableui VERSION 0.1.0 LANGUAGES C CXX)

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

# Core library -----------------------------------------------------------------

add_library(sableui SHARED src/sableui_api.cpp)
target_include_directories(sableui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_definitions(sableui PRIVATE SABLEUI_BUILDING)
target_link_libraries(sableui PRIVATE d2d1 dwrite user32 gdi32)
set_target_properties(sableui PROPERTIES OUTPUT_NAME "sableui")

if(NOT MSVC)
    target_compile_options(sableui PRIVATE -Wall -Wextra -Wpedantic -fms-extensions)
    target_link_options(sableui PRIVATE -static-libstdc++ -static-libgcc -static)
endif()

# Installation -----------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS sableui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/sableui/sableui.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sableui)
