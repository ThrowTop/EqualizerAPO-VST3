find_path(TCLAP_INCLUDE_DIR NAMES tclap/CmdLine.h REQUIRED)

add_executable(Benchmark Benchmark.cpp)
target_include_directories(Benchmark PRIVATE "${TCLAP_INCLUDE_DIR}")
target_link_libraries(Benchmark PRIVATE EqualizerAPO::Core)
target_precompile_headers(Benchmark PRIVATE
    "${PROJECT_SOURCE_DIR}/src/core/stdafx.h"
)
eapo_configure_target(Benchmark)
set_target_properties(Benchmark PROPERTIES FOLDER "Applications")
