cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CCACHE_ENABLE ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD OFF)
project(battery_monitor_display)

set(GDBINIT_CONNECT_PATH "${CMAKE_SOURCE_DIR}/gdbinit/connect")
include(cmake/gdbinit-custom.cmake)

set(CLANGD_GENERATOR_SCRIPT
    "${CMAKE_SOURCE_DIR}/cmake/update_clangd_from_compile_commands.cmake")

add_custom_target(update_clangd_from_compile_commands ALL
    COMMAND ${CMAKE_COMMAND}
            -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DPROJECT_BINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${CLANGD_GENERATOR_SCRIPT}
    COMMENT "Updating .clangd from compile_commands.json"
    VERBATIM
)