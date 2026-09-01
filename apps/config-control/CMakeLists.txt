add_executable(ConfigControl WIN32 main.cpp)
target_link_libraries(ConfigControl PRIVATE EqualizerAPO::Core shell32)
eapo_configure_target(ConfigControl)
set_target_properties(ConfigControl PROPERTIES FOLDER "Applications")

if(MSVC)
    target_link_options(ConfigControl PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
endif()
