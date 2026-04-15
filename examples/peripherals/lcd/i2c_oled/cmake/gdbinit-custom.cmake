set(generated_connect_path "${CMAKE_BINARY_DIR}/gdbinit/connect")
if(NOT DEFINED GDBINIT_CONNECT_PATH)
    set(GDBINIT_CONNECT_PATH "${CMAKE_SOURCE_DIR}/gdbinit/connect")
endif()
set(project_connect_path "${GDBINIT_CONNECT_PATH}")

file(READ "${generated_connect_path}" generated_connect_content)
string(REGEX REPLACE "(\r?\n)?continue\r?\n$" "" generated_connect_content
    "${generated_connect_content}")
string(REGEX REPLACE "(\r?\n)?target remote [^\r\n]+$" "" generated_connect_content
    "${generated_connect_content}")

if(EXISTS "${project_connect_path}")
    file(READ "${project_connect_path}" project_connect_content)
    file(WRITE "${generated_connect_path}"
        "${generated_connect_content}\n"
        "${project_connect_content}\n"
    )
else()
    file(WRITE "${generated_connect_path}" "${generated_connect_content}\n")
endif()
