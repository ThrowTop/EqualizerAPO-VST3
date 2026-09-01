file(GLOB_RECURSE eapo_core_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

list(APPEND eapo_core_sources
    "${PROJECT_SOURCE_DIR}/third_party/libHybridConv/libHybridConv_eapo.cpp"
    "${PROJECT_SOURCE_DIR}/third_party/libHybridConv/libHybridConv_eapo.h"
    "${EAPO_VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
)

add_library(eapo_core STATIC ${eapo_core_sources})
add_library(EqualizerAPO::Core ALIAS eapo_core)

target_include_directories(eapo_core PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/libHybridConv>"
)

target_link_libraries(eapo_core PUBLIC
    muparserx::muparserx
    FFTW3::fftw3f
    SndFile::sndfile
    sdk_hosting
    advapi32
    authz
    crypt32
    dbghelp
    ole32
    normaliz
    shlwapi
    taskschd
    version
    winmm
)

target_precompile_headers(eapo_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h"
)

eapo_configure_target(eapo_core)
eapo_configure_dsp_target(eapo_core)
set_target_properties(eapo_core PROPERTIES FOLDER "Libraries")
