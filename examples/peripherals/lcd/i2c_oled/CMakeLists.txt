cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CCACHE_ENABLE ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# Generate one build version shared by the application and bootloader sub-build.
set(KIWI_BUILD_VERSION_HEADER "${CMAKE_BINARY_DIR}/generated/kiwi_build_version.h")
idf_build_get_property(kiwi_extra_cmake_args EXTRA_CMAKE_ARGS)
list(APPEND kiwi_extra_cmake_args "-DKIWI_BUILD_VERSION_HEADER=${KIWI_BUILD_VERSION_HEADER}")
idf_build_set_property(EXTRA_CMAKE_ARGS "${kiwi_extra_cmake_args}")

project(battery_monitor_display)

add_custom_target(kiwi_generate_build_version
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND ${CMAKE_COMMAND}
            "-DOUTPUT_FILE=${KIWI_BUILD_VERSION_HEADER}"
            -P "${CMAKE_SOURCE_DIR}/cmake/generate_build_version.cmake"
    BYPRODUCTS "${KIWI_BUILD_VERSION_HEADER}"
    VERBATIM
)

idf_component_get_property(main_component_lib main COMPONENT_LIB)
target_include_directories(${main_component_lib} PRIVATE "${CMAKE_BINARY_DIR}/generated")
add_dependencies(${main_component_lib} kiwi_generate_build_version)
if(TARGET bootloader)
    add_dependencies(bootloader kiwi_generate_build_version)
endif()

# set(GDBINIT_CONNECT_PATH "${CMAKE_SOURCE_DIR}/gdbinit/connect")
# include(cmake/gdbinit-custom.cmake)

# set(CLANGD_GENERATOR_SCRIPT
#     "${CMAKE_SOURCE_DIR}/cmake/update_clangd_from_compile_commands.cmake")

# add_custom_target(update_clangd_from_compile_commands ALL
#     COMMAND ${CMAKE_COMMAND}
#             -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
#             -DPROJECT_BINARY_DIR=${CMAKE_BINARY_DIR}
#             -P ${CLANGD_GENERATOR_SCRIPT}
#     COMMENT "Updating .clangd from compile_commands.json"
#     VERBATIM
# )
