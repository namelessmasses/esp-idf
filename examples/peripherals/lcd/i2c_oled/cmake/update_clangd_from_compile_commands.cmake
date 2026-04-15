if(NOT DEFINED PROJECT_SOURCE_DIR OR NOT DEFINED PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "PROJECT_SOURCE_DIR and PROJECT_BINARY_DIR must be provided.")
endif()

set(compile_commands_path "${PROJECT_BINARY_DIR}/compile_commands.json")
set(clangd_path "${PROJECT_SOURCE_DIR}/.clangd")

if(NOT EXISTS "${compile_commands_path}")
    message(WARNING
        "compile_commands.json not found at ${compile_commands_path}; "
        "skipping .clangd update")
    return()
endif()

file(READ "${compile_commands_path}" compile_commands_raw)

set(include_dirs "")
# Extract all include directories passed as -I... for every compile command.
string(JSON compile_command_count LENGTH "${compile_commands_raw}")
math(EXPR compile_command_last_idx "${compile_command_count} - 1")

foreach(idx RANGE 0 ${compile_command_last_idx})
    string(JSON compile_command GET "${compile_commands_raw}" ${idx} command)
    string(REGEX MATCHALL "(^| )-I[^ ]+" include_tokens "${compile_command}")

    foreach(include_token IN LISTS include_tokens)
        string(REGEX REPLACE "(^| )-I" "" include_dir "${include_token}")
        file(TO_CMAKE_PATH "${include_dir}" include_dir)

        if(NOT IS_ABSOLUTE "${include_dir}")
            get_filename_component(
                include_dir "${include_dir}" ABSOLUTE
                BASE_DIR "${PROJECT_BINARY_DIR}")
            file(TO_CMAKE_PATH "${include_dir}" include_dir)
        endif()

        file(RELATIVE_PATH include_rel_to_src
            "${PROJECT_SOURCE_DIR}" "${include_dir}")
        if(NOT include_rel_to_src MATCHES "^\\.\\./")
            set(include_dir "${include_rel_to_src}")
        endif()

        list(APPEND include_dirs "${include_dir}")
    endforeach()
endforeach()

list(REMOVE_DUPLICATES include_dirs)
list(SORT include_dirs)

set(clangd_content "")
string(APPEND clangd_content
    "# Auto-generated from build/compile_commands.json by CMake.\n")
string(APPEND clangd_content "CompileFlags:\n")
string(APPEND clangd_content "  CompilationDatabase: build\n")
string(APPEND clangd_content "  Add:\n")
string(APPEND clangd_content "    - --target=xtensa-esp32-elf\n")

foreach(include_dir IN LISTS include_dirs)
    string(APPEND clangd_content "    - -I${include_dir}\n")
endforeach()

string(APPEND clangd_content "  Remove:\n")
string(APPEND clangd_content "    - -mlongcalls\n")
string(APPEND clangd_content "    - -fno-shrink-wrap\n")
string(APPEND clangd_content "    - -fno-jump-tables\n")
string(APPEND clangd_content "    - -fno-tree-switch-conversion\n")
string(APPEND clangd_content "    - -fstrict-volatile-bitfields\n")
string(APPEND clangd_content "    - -fuse-cxa-atexit\n")
string(APPEND clangd_content "    - -Wno-frame-address\n")
string(APPEND clangd_content "    - -Wno-old-style-declaration\n")
string(APPEND clangd_content "    - -fdiagnostics-color=*\n")

string(APPEND clangd_content "\nCompletion:\n")
string(APPEND clangd_content "  HeaderInsertion: Never\n")
string(APPEND clangd_content "\nDiagnostics:\n")
string(APPEND clangd_content "  UnusedIncludes: None\n")

file(WRITE "${clangd_path}" "${clangd_content}")

list(LENGTH include_dirs include_dir_count)
message(STATUS
    "Updated .clangd from compile_commands.json "
    "with ${include_dir_count} include directories")
