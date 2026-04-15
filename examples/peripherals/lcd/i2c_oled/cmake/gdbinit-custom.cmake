set(generated_connect_path "${CMAKE_BINARY_DIR}/gdbinit/connect")
if(NOT DEFINED GDBINIT_CONNECT_PATH)
    set(GDBINIT_CONNECT_PATH "${CMAKE_SOURCE_DIR}/gdbinit/connect")
endif()
set(project_connect_path "${GDBINIT_CONNECT_PATH}")

if(EXISTS "${project_connect_path}")
    message(STATUS "Using GDB connect script from ${project_connect_path}")
    file(READ "${project_connect_path}" project_connect_content)
    file(WRITE "${generated_connect_path}"
        "${project_connect_content}")
    message(STATUS "Generated GDB connect script at ${generated_connect_path}")
else()
    message(WARNING "GDB connect script not found at ${project_connect_path}; "
        "skipping generation of GDB connect script")
endif()

