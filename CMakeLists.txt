cmake_minimum_required(VERSION 3.10)

project(bsdiff)

add_library(bsdiff
    source/common.h
    source/common.c
    source/bsdiff.c
    source/bspatch.c
    3rdparty/bzip2/bzlib.c
    3rdparty/bzip2/compress.c
    3rdparty/bzip2/decompress.c
    3rdparty/bzip2/blocksort.c
    3rdparty/bzip2/crctable.c
    3rdparty/bzip2/huffman.c
    3rdparty/bzip2/randtable.c)
target_include_directories(bsdiff
    PUBLIC "3rdparty/bzip2"
    PUBLIC "include")
if (MSVC)
    target_compile_definitions(bsdiff PUBLIC "_CRT_SECURE_NO_WARNINGS")
endif()

add_executable(bsdiff_diff source/bsdiff_diff.c)
target_include_directories(bsdiff_diff PUBLIC "include")
target_link_libraries(bsdiff_diff bsdiff)

add_executable(bsdiff_patch source/bsdiff_patch.c)
target_include_directories(bsdiff_patch PUBLIC "include")
target_link_libraries(bsdiff_patch bsdiff)
