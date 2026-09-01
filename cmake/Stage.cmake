if(NOT DEFINED build_dir OR NOT DEFINED install_dir OR NOT DEFINED config)
    message(FATAL_ERROR "Stage.cmake requires build_dir, install_dir, and config")
endif()

cmake_path(NORMAL_PATH install_dir OUTPUT_VARIABLE normalized_install_dir)
cmake_path(NORMAL_PATH CMAKE_CURRENT_LIST_DIR OUTPUT_VARIABLE cmake_dir)
cmake_path(GET cmake_dir PARENT_PATH source_dir)

if(normalized_install_dir STREQUAL "" OR normalized_install_dir STREQUAL source_dir)
    message(FATAL_ERROR "Refusing to stage into an unsafe directory: ${normalized_install_dir}")
endif()

set(staging_dir "${normalized_install_dir}.staging")
set(previous_dir "${normalized_install_dir}.previous")

file(REMOVE_RECURSE "${staging_dir}" "${previous_dir}")
if(EXISTS "${staging_dir}" OR EXISTS "${previous_dir}")
    message(FATAL_ERROR "Unable to clean temporary staging directories beside ${normalized_install_dir}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${build_dir}"
        --config "${config}"
        --prefix "${staging_dir}"
    COMMAND_ERROR_IS_FATAL ANY
)

if(EXISTS "${normalized_install_dir}")
    file(RENAME "${normalized_install_dir}" "${previous_dir}"
        RESULT move_existing_result)
    if(NOT move_existing_result STREQUAL "0")
        file(REMOVE_RECURSE "${staging_dir}")
        message(FATAL_ERROR
            "Unable to replace ${normalized_install_dir}: ${move_existing_result}\n"
            "The staged runtime is probably installed or running. Close its "
            "applications and restart Windows Audio (or reboot), then stage again.")
    endif()
endif()

file(RENAME "${staging_dir}" "${normalized_install_dir}"
    RESULT activate_staging_result)
if(NOT activate_staging_result STREQUAL "0")
    if(EXISTS "${previous_dir}" AND NOT EXISTS "${normalized_install_dir}")
        file(RENAME "${previous_dir}" "${normalized_install_dir}")
    endif()
    message(FATAL_ERROR
        "Unable to activate staged runtime at ${normalized_install_dir}: ${activate_staging_result}")
endif()

file(REMOVE_RECURSE "${previous_dir}")
