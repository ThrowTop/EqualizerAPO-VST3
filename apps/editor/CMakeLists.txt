file(GLOB_RECURSE editor_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rc"
)

set(CMAKE_AUTORCC ON)
qt_add_executable(Editor WIN32 ${editor_sources})

set(editor_ts_files
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/Editor_de.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/Editor_fr.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/Editor_zh_CN.ts"
)
qt_add_lrelease(Editor
    TS_FILES ${editor_ts_files}
    LRELEASE_TARGET Editor_lrelease
    QM_FILES_OUTPUT_VARIABLE editor_qm_files
)
foreach(editor_qm_file IN LISTS editor_qm_files)
    cmake_path(GET editor_qm_file FILENAME editor_qm_name)
    set_source_files_properties("${editor_qm_file}" PROPERTIES
        QT_RESOURCE_ALIAS "translations/${editor_qm_name}"
    )
endforeach()
qt_add_resources(Editor editor_translations
    PREFIX "/"
    FILES ${editor_qm_files}
)
qt_add_lupdate(Editor
    TS_FILES ${editor_ts_files}
    LUPDATE_TARGET update-editor-translations
)

target_include_directories(Editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/apps"
)
target_link_libraries(Editor PRIVATE
    EqualizerAPO::Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
target_precompile_headers(Editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stable.h"
)
eapo_configure_target(Editor)
set_target_properties(Editor PROPERTIES FOLDER "Applications")
