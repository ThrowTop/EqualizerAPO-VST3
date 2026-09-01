cmake_minimum_required(VERSION 4.4)

project(
    EqualizerAPO_VST3
    VERSION 0.1.0
    DESCRIPTION "EqualizerAPO-VST3 system-wide audio processor for Windows"
    LANGUAGES C CXX RC
)

if(NOT WIN32)
    message(FATAL_ERROR "EqualizerAPO-VST3 can only be built for Windows.")
endif()

include(CheckIPOSupported)
include(GNUInstallDirs)
include(CTest)

# All installed runtime files deliberately live beside the executables.
set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Runtime executable directory" FORCE)
set(CMAKE_INSTALL_LIBDIR "." CACHE PATH "Runtime library directory" FORCE)
set(CMAKE_INSTALL_LIBEXECDIR "." CACHE PATH "Runtime helper directory" FORCE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Keep runnable compiler outputs together so utilities which launch adjacent
# helpers behave the same way in Visual Studio as they do in the live runtime.
# Per-configuration properties avoid Visual Studio adding another ambiguous
# Debug or Release directory above these explicit paths.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/compiled/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/compiled/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/compiled/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/compiled/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/.intermediate/libraries/debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/.intermediate/libraries/release")

# Visual Studio necessarily generates project mirrors and *.dir intermediate
# directories. Import this MSBuild hook for every generated project so those
# implementation details stay hidden in Explorer.
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/Directory.Build.targets.in"
    "${CMAKE_BINARY_DIR}/Directory.Build.targets"
    @ONLY
)
execute_process(
    COMMAND attrib.exe +h "${CMAKE_BINARY_DIR}/Directory.Build.targets"
    OUTPUT_QUIET ERROR_QUIET
)

# Match the original Visual Studio solution: one tree, Debug and Release only.
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING
        "Supported build configurations" FORCE)
endif()

option(EAPO_BUILD_EDITOR "Build the Configuration Editor" ON)
option(EAPO_BUILD_BENCHMARK "Build the developer-only DSP benchmark" OFF)
option(EAPO_BUILD_VOICEMEETER_CLIENT "Build Voicemeeter integration" ON)
option(EAPO_BUILD_UPDATE_CHECKER "Build the upstream update checker" OFF)

set(EAPO_VST3_SDK_ROOT "${PROJECT_SOURCE_DIR}/.deps/vst3sdk" CACHE PATH
    "Pinned Steinberg VST3 SDK checkout")
if(NOT EXISTS "${EAPO_VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "VST3 SDK not found at ${EAPO_VST3_SDK_ROOT}. Run tools/bootstrap-dependencies.ps1.")
endif()

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
add_subdirectory("${EAPO_VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/.vst3sdk" EXCLUDE_FROM_ALL)

set(EAPO_SIMD "BASELINE" CACHE STRING
    "DSP instruction set: BASELINE, SSE2, AVX, AVX2, AVX512, or AVX512_256")
set(EAPO_DEV_CONFIG_FILE "" CACHE FILEPATH
    "Absolute .txt entry file selected by the development deploy target")
set_property(CACHE EAPO_SIMD PROPERTY STRINGS
    BASELINE SSE2 AVX AVX2 AVX512 AVX512_256)

check_ipo_supported(RESULT EAPO_IPO_SUPPORTED OUTPUT EAPO_IPO_ERROR)
if(NOT EAPO_IPO_SUPPORTED)
    message(WARNING "Release IPO is unavailable: ${EAPO_IPO_ERROR}")
endif()

function(eapo_configure_target target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)

    if(MSVC)
        target_compile_options(${target} PRIVATE /MP /utf-8 /W4 /Zc:__cplusplus)
    endif()

    if(EAPO_IPO_SUPPORTED)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()

    set_target_properties(${target} PROPERTIES
        FOLDER "EqualizerAPO-VST3"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
    )

    if(MSVC)
        set(eapo_debugger_path "%PATH%")
        if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
            set(eapo_vcpkg_debugger_bin
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/bin,bin>")
            set(eapo_debugger_path
                "${eapo_vcpkg_debugger_bin}$<SEMICOLON>${eapo_debugger_path}")
        endif()
        if(DEFINED Qt6_DIR)
            set(eapo_qt_debugger_bin "${Qt6_DIR}/../../../bin")
            set(eapo_debugger_path
                "${eapo_qt_debugger_bin}$<SEMICOLON>${eapo_debugger_path}")
        endif()
        set_property(TARGET ${target} PROPERTY
            VS_DEBUGGER_ENVIRONMENT "PATH=${eapo_debugger_path}")
    endif()
endfunction()

function(eapo_configure_dsp_target target)
    string(TOUPPER "${EAPO_SIMD}" simd)

    if(NOT simd IN_LIST EAPO_VALID_SIMD_VALUES)
        message(FATAL_ERROR "Unsupported EAPO_SIMD value: ${EAPO_SIMD}")
    endif()

    if(MSVC)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND simd STREQUAL "SSE2")
            target_compile_options(${target} PRIVATE /arch:SSE2)
        elseif(simd STREQUAL "AVX")
            target_compile_options(${target} PRIVATE /arch:AVX)
        elseif(simd STREQUAL "AVX2")
            target_compile_options(${target} PRIVATE /arch:AVX2)
        elseif(simd STREQUAL "AVX512")
            target_compile_options(${target} PRIVATE /arch:AVX512)
        elseif(simd STREQUAL "AVX512_256")
            target_compile_options(${target} PRIVATE /arch:AVX512 /vlen=256)
        endif()
    endif()
endfunction()

set(EAPO_VALID_SIMD_VALUES BASELINE SSE2 AVX AVX2 AVX512 AVX512_256)

if(NOT EAPO_SIMD IN_LIST EAPO_VALID_SIMD_VALUES)
    message(FATAL_ERROR "EAPO_SIMD must be one of: ${EAPO_VALID_SIMD_VALUES}")
endif()

find_package(FFTW3f CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)

if(EAPO_BUILD_EDITOR OR EAPO_BUILD_UPDATE_CHECKER)
    # The runtime is intentionally flat, with Qt plugins under its qt/ folder.
    set(QT_DEPLOY_BIN_DIR ".")
    set(QT_DEPLOY_LIB_DIR ".")
    set(QT_DEPLOY_PLUGINS_DIR "qt")
    set(eapo_qt_components Core Gui Widgets Svg)
    if(EAPO_BUILD_EDITOR)
        list(APPEND eapo_qt_components LinguistTools)
    endif()
    if(EAPO_BUILD_UPDATE_CHECKER)
        list(APPEND eapo_qt_components Network)
    endif()
    find_package(Qt6 6.7 REQUIRED COMPONENTS ${eapo_qt_components})
    qt_standard_project_setup()
endif()

add_subdirectory(third_party/muparserx)
add_subdirectory(src/core)
add_subdirectory(src/apo)
add_subdirectory(apps/config-control)
add_subdirectory(apps/device-control)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(EAPO_BUILD_EDITOR)
    add_subdirectory(apps/editor)
endif()
if(EAPO_BUILD_BENCHMARK)
    add_subdirectory(apps/benchmark)
endif()
if(EAPO_BUILD_VOICEMEETER_CLIENT)
    add_subdirectory(apps/voicemeeter-client)
endif()
if(EAPO_BUILD_UPDATE_CHECKER)
    add_subdirectory(apps/update-checker)
endif()

if(TARGET Editor)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Editor)
endif()

set(eapo_runtime_targets EqualizerAPO ConfigControl DeviceControl)
foreach(optional_target IN ITEMS
        Editor Benchmark VoicemeeterClient UpdateChecker)
    if(TARGET ${optional_target})
        list(APPEND eapo_runtime_targets ${optional_target})
    endif()
endforeach()

install(
    TARGETS ${eapo_runtime_targets}
    RUNTIME_DEPENDENCY_SET eapo_runtime_dependencies
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(eapo_vcpkg_release_bin
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    set(eapo_vcpkg_debug_bin
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
else()
    set(eapo_vcpkg_release_bin "")
    set(eapo_vcpkg_debug_bin "")
endif()

install(
    RUNTIME_DEPENDENCY_SET eapo_runtime_dependencies
    DESTINATION .
    DIRECTORIES
        "$<$<CONFIG:Debug>:${eapo_vcpkg_debug_bin}>"
        "$<$<NOT:$<CONFIG:Debug>>:${eapo_vcpkg_release_bin}>"
        "${Qt6_DIR}/../../../bin"
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES
        ".*[\\\\/]system32[\\\\/].*"
        ".*[\\\\/]SysWOW64[\\\\/].*"
)

install(DIRECTORY DESTINATION VSTPlugins)
install(DIRECTORY DESTINATION VST3)
install(FILES
    packaging/windows/install.ps1
    packaging/windows/install.bat
    THIRD_PARTY_NOTICES.md
    License.txt
    DESTINATION .
)
install(FILES
    packaging/windows/README.txt
    DESTINATION .
    RENAME README.txt
)
install(FILES
    "${EAPO_VST3_SDK_ROOT}/LICENSE.txt"
    DESTINATION licenses
    RENAME VST3_SDK_LICENSE.txt
)
install(FILES
    packaging/licenses/LGPL-3.0-only.txt
    packaging/licenses/LGPL-2.1-only.txt
    DESTINATION licenses
)
install(FILES
    third_party/muparserx/LICENSE.txt
    DESTINATION licenses
    RENAME muparserx-LICENSE.txt
)
# vcpkg records the exact notices for the libraries and codec dependencies
# selected by the pinned manifest. Copy every notice that exists in this build
# so binary packages remain self-describing when dependency resolution changes.
foreach(eapo_dependency IN ITEMS
        fftw3 libsndfile libflac libogg libvorbis mp3lame mpg123 opus)
    set(eapo_dependency_notice
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${eapo_dependency}/copyright")
    if(EXISTS "${eapo_dependency_notice}")
        install(FILES "${eapo_dependency_notice}"
            DESTINATION licenses
            RENAME "${eapo_dependency}-LICENSE.txt")
    endif()
endforeach()

if(TARGET Editor)
    set(eapo_qt_deploy_target Editor)
elseif(TARGET UpdateChecker)
    set(eapo_qt_deploy_target UpdateChecker)
endif()

if(DEFINED eapo_qt_deploy_target)
    qt_generate_deploy_app_script(
        TARGET ${eapo_qt_deploy_target}
        OUTPUT_SCRIPT eapo_qt_deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        DEPLOY_TOOL_OPTIONS
            --no-system-d3d-compiler
            --no-system-dxc-compiler
            --no-opengl-sw
            --skip-plugin-types generic,networkinformation
            --exclude-plugins qgif,qjpeg,qcertonlybackend,qopensslbackend
    )
    install(CODE "
        set(QT_DEPLOY_BIN_DIR \".\")
        set(QT_DEPLOY_LIB_DIR \".\")
        set(QT_DEPLOY_PLUGINS_DIR \"qt\")
        include(\"${eapo_qt_deploy_script}\")
    ")
endif()

# Packaging and deployment staging is temporary. Successful targets remove it;
# only the registered live runtime and finished dist artifacts persist.
set(EAPO_STAGING_ROOT "${CMAKE_BINARY_DIR}/.staging")
set(EAPO_DEV_STAGE_DIR "${EAPO_STAGING_ROOT}/deploy")
set(EAPO_ZIP_STAGE_DIR "${EAPO_STAGING_ROOT}/portable-zip")
set(EAPO_INSTALLER_STAGE_DIR "${EAPO_STAGING_ROOT}/installer")
set(EAPO_LIVE_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/build/live-runtime")
set(EAPO_DIST_DIR "${PROJECT_SOURCE_DIR}/dist")
set(EAPO_ARCHIVE_NAME "EqualizerAPO-VST3-${PROJECT_VERSION}.zip")

add_custom_target(package-standalone
    COMMAND "${CMAKE_COMMAND}"
        -Dbuild_dir=${CMAKE_BINARY_DIR}
        -Dinstall_dir=${EAPO_ZIP_STAGE_DIR}
        -Dconfig=$<CONFIG>
        -P "${PROJECT_SOURCE_DIR}/cmake/Stage.cmake"
    COMMAND "${CMAKE_COMMAND}"
        -Dsource_dir=${EAPO_ZIP_STAGE_DIR}
        -Ddist_dir=${EAPO_DIST_DIR}
        -Darchive_name=${EAPO_ARCHIVE_NAME}
        -Dremove_source_after=ON
        -P "${PROJECT_SOURCE_DIR}/cmake/Archive.cmake"
    DEPENDS ${eapo_runtime_targets}
    USES_TERMINAL
    COMMENT "Building the portable ZIP in ${EAPO_DIST_DIR}"
)

add_custom_target(package-installer
    COMMAND "${CMAKE_COMMAND}"
        -Dbuild_dir=${CMAKE_BINARY_DIR}
        -Dinstall_dir=${EAPO_INSTALLER_STAGE_DIR}
        -Dconfig=$<CONFIG>
        -P "${PROJECT_SOURCE_DIR}/cmake/Stage.cmake"
    COMMAND "${CMAKE_COMMAND}"
        -Dstage_dir=${EAPO_INSTALLER_STAGE_DIR}
        -Ddist_dir=${EAPO_DIST_DIR}
        -Dsource_dir=${PROJECT_SOURCE_DIR}
        -Dversion=${PROJECT_VERSION}
        -Dremove_stage_after=ON
        -P "${PROJECT_SOURCE_DIR}/cmake/BuildInstaller.cmake"
    DEPENDS ${eapo_runtime_targets}
    USES_TERMINAL
    COMMENT "Building the full Windows installer with NSIS"
)

add_custom_target(package-release
    DEPENDS package-standalone package-installer
)

set(EAPO_DEPLOY_COMMAND
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass
    -File "${PROJECT_SOURCE_DIR}/cmake/DeployDev.ps1"
    -SourceDir "${EAPO_DEV_STAGE_DIR}"
    -InstallDir "${EAPO_LIVE_RUNTIME_DIR}"
)
if(EAPO_DEV_CONFIG_FILE)
    list(APPEND EAPO_DEPLOY_COMMAND -ConfigFile "${EAPO_DEV_CONFIG_FILE}")
endif()

add_custom_target(deploy-dev
    COMMAND "${CMAKE_COMMAND}"
        -Dbuild_dir=${CMAKE_BINARY_DIR}
        -Dinstall_dir=${EAPO_DEV_STAGE_DIR}
        -Dconfig=$<CONFIG>
        -P "${PROJECT_SOURCE_DIR}/cmake/Stage.cmake"
    COMMAND ${EAPO_DEPLOY_COMMAND}
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${EAPO_DEV_STAGE_DIR}"
    DEPENDS ${eapo_runtime_targets}
    USES_TERMINAL
    COMMENT "Deploying the current build and restarting Windows Audio"
)

message(STATUS "EqualizerAPO-VST3 ${PROJECT_VERSION}")
message(STATUS "Target architecture: ${CMAKE_VS_PLATFORM_NAME}")
message(STATUS "DSP instruction set: ${EAPO_SIMD}")
