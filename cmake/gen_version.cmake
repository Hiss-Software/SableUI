# Writes sableui_version.h with the version string taken from the latest
# git tag. The file is rewritten only when its content changes, so unrelated
# targets are not rebuilt on every configure.

set(VER "0.0.0-unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} update-index -q --refresh
        WORKING_DIRECTORY ${GIT_DIR}
        OUTPUT_QUIET ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty
        WORKING_DIRECTORY ${GIT_DIR}
        OUTPUT_VARIABLE VER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT VER)
    set(VER "0.0.0-unknown")
endif()

# v0.2.0-1-g5887e40 -> 0.2.0
if(VER MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(VER_NUM "${CMAKE_MATCH_1}")
else()
    set(VER_NUM "0.0.0")
endif()

set(content
    "#define SABLEUI_VERSION \"${VER}\"\n#define SABLEUI_VERSION_NUM ${VER_NUM}\n")

if(EXISTS "${OUT_FILE}")
    file(READ "${OUT_FILE}" prev)
else()
    set(prev "")
endif()
if(NOT prev STREQUAL content)
    file(WRITE "${OUT_FILE}" "${content}")
endif()
