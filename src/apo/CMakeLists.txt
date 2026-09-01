set(eapo_apo_sources
    ClassFactory.cpp
    ClassFactory.h
    DllMain.cpp
    EqualizerAPO.cpp
    EqualizerAPO.def
    EqualizerAPO.h
    resource.h
)

if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    list(APPEND eapo_apo_sources EqualizerAPO-ARM64.rc)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND eapo_apo_sources EqualizerAPO64.rc)
else()
    list(APPEND eapo_apo_sources EqualizerAPO.rc)
endif()

add_library(EqualizerAPO SHARED ${eapo_apo_sources})

target_compile_definitions(EqualizerAPO PRIVATE
    EQUALIZERAPO_EXPORTS
    USE_WINDDK
)
target_link_libraries(EqualizerAPO PRIVATE
    EqualizerAPO::Core
    AudioBaseProcessingObject
    audiomediatypecrt
    audioeng
    legacy_stdio_definitions
    wmcodecdspuuid
)
target_precompile_headers(EqualizerAPO PRIVATE
    "${PROJECT_SOURCE_DIR}/src/core/stdafx.h"
)

eapo_configure_target(EqualizerAPO)
set_target_properties(EqualizerAPO PROPERTIES
    OUTPUT_NAME "EqualizerAPO"
    PREFIX ""
    FOLDER "Runtime"
)
