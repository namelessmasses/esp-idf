set(generated_connect_path "${CMAKE_BINARY_DIR}/gdbinit")
if(NOT DEFINED GDBINIT_CONNECT_PATH)
    set(GDBINIT_CONNECT_PATH "${CMAKE_SOURCE_DIR}/gdbinit")
endif()
set(project_connect_path "${GDBINIT_CONNECT_PATH}")

if(EXISTS "${project_connect_path}")
    message(STATUS "Using GDB init scripts from ${project_connect_path}")
    file(REMOVE_RECURSE "${generated_connect_path}")
    file(MAKE_DIRECTORY "${generated_connect_path}")
    file(GLOB gdb_connect_files "${project_connect_path}/*")
    file(COPY ${gdb_connect_files} DESTINATION "${generated_connect_path}")
    message(STATUS "Generated GDB init script at ${generated_connect_path}")
else()
    message(WARNING "GDB init script not found at ${project_connect_path}; "
        "skipping generation of GDB init script")
endif()

