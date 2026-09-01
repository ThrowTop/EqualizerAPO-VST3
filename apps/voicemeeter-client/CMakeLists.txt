add_executable(VoicemeeterClient WIN32
    resource.h
    VoicemeeterClient.cpp
    VoicemeeterClient.h
    VoicemeeterClient.rc
    VoicemeeterRemote.h
)
target_link_libraries(VoicemeeterClient PRIVATE EqualizerAPO::Core)
target_precompile_headers(VoicemeeterClient PRIVATE
    "${PROJECT_SOURCE_DIR}/src/core/stdafx.h"
)
eapo_configure_target(VoicemeeterClient)
set_target_properties(VoicemeeterClient PROPERTIES FOLDER "Applications")
