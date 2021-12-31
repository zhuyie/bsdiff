cmake_minimum_required(VERSION 3.10)

project(bsdiff)

option(BUILD_SHARED_LIBS "Set to ON to build shared libraries" OFF)

# bzip2
add_library(bzip2 STATIC
    3rdparty/bzip2/bzlib.c
    3rdparty/bzip2/compress.c
    3rdparty/bzip2/decompress.c
    3rdparty/bzip2/blocksort.c
    3rdparty/bzip2/crctable.c
    3rdparty/bzip2/huffman.c
    3rdparty/bzip2/randtable.c)

# libdivsufsort
function(add_libdivsufsort)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_DIVSUFSORT64 ON)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(3rdparty/libdivsufsort)
endfunction()

add_libdivsufsort()

# bsdiff
add_library(bsdiff
    source/misc.h
    source/misc.c
    source/file_stream.c
    source/sub_stream.h
    source/sub_stream.c
    source/bz2_compress.c
    source/bz2_decompress.c
    source/bsdiff.c
    source/bspatch.c)
target_include_directories(bsdiff
    PUBLIC "3rdparty/bzip2"
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libdivsufsort/include"
    PUBLIC "include")
if (MSVC)
    target_compile_definitions(bsdiff PUBLIC "_CRT_SECURE_NO_WARNINGS")
endif()
target_link_libraries(bsdiff bzip2 divsufsort divsufsort64)

# bsdiff_diff
add_executable(bsdiff_diff source/bsdiff_diff.c)
target_include_directories(bsdiff_diff PUBLIC "include")
target_link_libraries(bsdiff_diff bsdiff)

# bsdiff_patch
add_executable(bsdiff_patch source/bsdiff_patch.c)
target_include_directories(bsdiff_patch PUBLIC "include")
target_link_libraries(bsdiff_patch bsdiff)
