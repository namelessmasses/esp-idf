if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE must be set")
endif()

string(TIMESTAMP build_version "%y%m%d%H%M")
if(build_version GREATER 4294967295)
    message(FATAL_ERROR "Build timestamp version does not fit in the uint32_t bootloader version field")
endif()
file(WRITE "${OUTPUT_FILE}"
    "#pragma once\n#define KIWI_BUILD_YYMMDDHHMM ${build_version}U\n")
