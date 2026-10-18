idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return() # This component is not supported by the POSIX/Linux simulator
endif()

idf_component_register(SRCS "esp_bootloader_desc.c"
                       INCLUDE_DIRS "include")

if(CONFIG_BOOTLOADER_PROJECT_VER_FROM_BUILD_TIMESTAMP)
    if(NOT KIWI_BUILD_VERSION_HEADER)
        message(FATAL_ERROR "KIWI_BUILD_VERSION_HEADER must be passed to the bootloader sub-build")
    endif()
    get_filename_component(kiwi_build_version_include_dir "${KIWI_BUILD_VERSION_HEADER}" DIRECTORY)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${kiwi_build_version_include_dir}")
endif()

if(BOOTLOADER_BUILD)
    # esp_bootloader_desc structure is added as an undefined symbol because otherwise the
    # linker will ignore this structure as it has no other files depending on it.
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u esp_bootloader_desc")

    idf_build_get_property(project_name PROJECT_NAME)
    if(CONFIG_BOOTLOADER_PROJECT_VER_FROM_BUILD_TIMESTAMP)
        set(project_version "YYMMDDHHMM (generated for this build)")
    else()
        set(project_version "${CONFIG_BOOTLOADER_PROJECT_VER}")
    endif()
    message(STATUS "Bootloader project name: \"${project_name}\" version: ${project_version}")
endif()
