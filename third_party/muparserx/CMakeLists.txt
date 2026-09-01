file(GLOB muparserx_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.h"
)

add_library(muparserx STATIC ${muparserx_sources})
add_library(muparserx::muparserx ALIAS muparserx)

target_include_directories(muparserx PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parser>"
)
target_compile_definitions(muparserx PRIVATE UNICODE _UNICODE _LIB)
target_compile_features(muparserx PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(muparserx PRIVATE /MP /utf-8 /W3)
endif()

set_target_properties(muparserx PROPERTIES
    FOLDER "Third Party"
    POSITION_INDEPENDENT_CODE ON
)
