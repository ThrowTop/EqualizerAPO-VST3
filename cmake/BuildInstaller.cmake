if(NOT DEFINED stage_dir OR NOT DEFINED dist_dir OR NOT DEFINED source_dir OR NOT DEFINED version)
    message(FATAL_ERROR "BuildInstaller.cmake requires stage_dir, dist_dir, source_dir, and version")
endif()

find_program(MAKENSIS_EXECUTABLE
    NAMES makensis.exe makensis
    HINTS
        "$ENV{ProgramFiles}/NSIS"
        "C:/Program Files (x86)/NSIS"
)
if(NOT MAKENSIS_EXECUTABLE)
    if(remove_stage_after)
        file(REMOVE_RECURSE "${stage_dir}")
    endif()
    message(FATAL_ERROR
        "NSIS 3 is required for the installer target. Install it from https://nsis.sourceforge.io/Download.")
endif()

file(MAKE_DIRECTORY "${dist_dir}")
set(installer_path "${dist_dir}/EqualizerAPO-VST3-${version}-setup.exe")
execute_process(
    COMMAND "${MAKENSIS_EXECUTABLE}"
        "/DVERSION=${version}"
        "/DSTAGE_DIR=${stage_dir}"
        "/DOUT_FILE=${installer_path}"
        "${source_dir}/packaging/windows/EqualizerAPO.nsi"
    RESULT_VARIABLE installer_result
)
if(remove_stage_after)
    file(REMOVE_RECURSE "${stage_dir}")
endif()
if(NOT installer_result EQUAL 0)
    message(FATAL_ERROR "NSIS failed with exit code ${installer_result}")
endif()
