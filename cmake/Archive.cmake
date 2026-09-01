if(NOT DEFINED source_dir OR NOT DEFINED dist_dir OR NOT DEFINED archive_name)
    message(FATAL_ERROR "Archive.cmake requires source_dir, dist_dir, and archive_name")
endif()

if(NOT EXISTS "${source_dir}/EqualizerAPO.dll")
    message(FATAL_ERROR "Standalone output is incomplete: ${source_dir}")
endif()

file(MAKE_DIRECTORY "${dist_dir}")
set(archive_path "${dist_dir}/${archive_name}")
set(temporary_archive_path "${archive_path}.tmp")
file(REMOVE "${temporary_archive_path}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${temporary_archive_path}" --format=zip -- .
    WORKING_DIRECTORY "${source_dir}"
    RESULT_VARIABLE archive_result
)
if(remove_source_after)
    file(REMOVE_RECURSE "${source_dir}")
endif()
if(NOT archive_result EQUAL 0)
    file(REMOVE "${temporary_archive_path}")
    message(FATAL_ERROR "Unable to create portable archive: ${archive_result}")
endif()
file(RENAME "${temporary_archive_path}" "${archive_path}" RESULT rename_result)
if(NOT rename_result STREQUAL "0")
    message(FATAL_ERROR "Unable to replace ${archive_path}: ${rename_result}")
endif()
