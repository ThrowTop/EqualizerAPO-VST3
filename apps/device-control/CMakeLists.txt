add_executable(DeviceControl WIN32 main.cpp)
target_link_libraries(DeviceControl PRIVATE EqualizerAPO::Core shell32 ws2_32)
eapo_configure_target(DeviceControl)
set_target_properties(DeviceControl PROPERTIES FOLDER "Applications")

if(MSVC)
    target_link_options(DeviceControl PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
endif()
