add_executable(
    test_bsdiff_unit
    test_stream_memory.cpp
    test_bsdiff_api.cpp
    test_bspatch_api.cpp
)

target_link_libraries(
    test_bsdiff_unit
    gtest_main
    bsdiff
)

target_include_directories(
    test_bsdiff_unit
    PRIVATE "../include"
)

include(GoogleTest)
gtest_discover_tests(test_bsdiff_unit)

# Benchmark test executable
add_executable(
    test_bsdiff_benchmark
    test_bsdiff_benchmark.cpp
)

target_link_libraries(
    test_bsdiff_benchmark
    benchmark::benchmark
    bsdiff
)

target_include_directories(
    test_bsdiff_benchmark
    PRIVATE "../include"
)

# Compatibility test with original bsdiff-4.3
# Build the original bsdiff tools first (done outside CMake via bsdiff-orig/build/).
set(ORIG_BSDIFF_BUILD_DIR "${CMAKE_SOURCE_DIR}/3rdparty/bsdiff-orig/build")
add_executable(
    test_compat_orig
    test_compat_orig.cpp
)
target_link_libraries(
    test_compat_orig
    gtest_main
    bsdiff
)
target_include_directories(
    test_compat_orig
    PRIVATE "../include"
)
target_compile_definitions(
    test_compat_orig
    PRIVATE "ORIG_BSDIFF_DIR=\"${ORIG_BSDIFF_BUILD_DIR}\""
)
gtest_discover_tests(test_compat_orig
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
