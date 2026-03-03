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
