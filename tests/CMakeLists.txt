add_executable(StableDeviceBindingsTests
    StableDeviceBindingsTests.cpp
)

target_link_libraries(StableDeviceBindingsTests PRIVATE EqualizerAPO::Core)
eapo_configure_target(StableDeviceBindingsTests)
set_target_properties(StableDeviceBindingsTests PROPERTIES FOLDER "Tests")

add_test(NAME StableDeviceBindings COMMAND StableDeviceBindingsTests)

set(eapo_vst3_test_plugin_sources
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/adelaycontroller.cpp"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/adelaycontroller.h"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/adelayids.h"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/adelayprocessor.cpp"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/adelayprocessor.h"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/factory.cpp"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/samples/vst/adelay/source/version.h"
)
smtg_add_vst3plugin(EapoVST3TestPlugin PACKAGE_NAME "EapoVST3TestPlugin"
    SOURCES_LIST ${eapo_vst3_test_plugin_sources})
smtg_target_configure_version_file(EapoVST3TestPlugin)
target_include_directories(EapoVST3TestPlugin PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(EapoVST3TestPlugin PRIVATE sdk)
set_target_properties(EapoVST3TestPlugin PROPERTIES FOLDER "Tests")

get_target_property(eapo_vst3_test_bundle EapoVST3TestPlugin SMTG_PLUGIN_PACKAGE_PATH)
add_executable(VST3HostTests VST3HostTests.cpp)
target_link_libraries(VST3HostTests PRIVATE EqualizerAPO::Core)
target_compile_definitions(VST3HostTests PRIVATE
    EAPO_VST3_TEST_BUNDLE=L"${eapo_vst3_test_bundle}")
add_dependencies(VST3HostTests EapoVST3TestPlugin)
eapo_configure_target(VST3HostTests)
set_target_properties(VST3HostTests PROPERTIES FOLDER "Tests")
add_test(NAME VST3Host COMMAND VST3HostTests)
