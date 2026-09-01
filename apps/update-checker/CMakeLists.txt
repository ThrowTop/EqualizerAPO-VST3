file(GLOB_RECURSE update_checker_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rc"
)

qt_add_executable(UpdateChecker WIN32 ${update_checker_sources})
target_include_directories(UpdateChecker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(UpdateChecker PRIVATE
    EqualizerAPO::Core
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    secur32
)
target_precompile_headers(UpdateChecker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h"
)
eapo_configure_target(UpdateChecker)
set_target_properties(UpdateChecker PROPERTIES FOLDER "Applications")
