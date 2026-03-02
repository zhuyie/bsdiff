add_executable(
    test_bsdiff_unit
    test_stream_memory.cpp
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
