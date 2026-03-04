cmake_minimum_required(VERSION 3.10)

# Allow 3rdparty CMakeLists.txt files with older cmake_minimum_required versions
# to work with newer CMake without modifying them.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(bsdiff C CXX)

include(CTest)

option(BUILD_SHARED_LIBS "Set to ON to build shared libraries" OFF)
option(BUILD_STANDALONES "Set to OFF to not build standalones" ON)

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

# Zstd
function(add_zstd)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_TESTS    OFF)
    set(ZSTD_BUILD_SHARED   OFF)
    add_subdirectory(3rdparty/zstd/build/cmake)
endfunction()
add_zstd()

# bsdiff
add_library(bsdiff
    source/bsdiff_private.h
    source/misc.c
    source/stream_file.c
    source/stream_memory.c
    source/stream_sub.c
    source/compressor_bz2.c
    source/decompressor_bz2.c
    source/patch_packer_bz2.c
    source/compressor_zstd.c
    source/decompressor_zstd.c
    source/patch_packer_zstd.c
    source/bsdiff.c
    source/bspatch.c)
target_include_directories(bsdiff
    PRIVATE "3rdparty/bzip2"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libdivsufsort/include"
    PRIVATE "3rdparty/zstd/lib"
    PRIVATE "include")
if (BUILD_SHARED_LIBS)
    target_compile_definitions(bsdiff PRIVATE "BSDIFF_DLL" "BSDIFF_EXPORTS")
endif()
if (MSVC)
    target_compile_definitions(bsdiff PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()
target_link_libraries(bsdiff PRIVATE bzip2 PRIVATE divsufsort PRIVATE divsufsort64 PRIVATE libzstd_static)

if (BUILD_STANDALONES)
    # bsdiff_app
    add_executable(bsdiff_app source/bsdiff_app.c)
    set_target_properties(bsdiff_app PROPERTIES OUTPUT_NAME "bsdiff")
    target_include_directories(bsdiff_app PRIVATE "include")
    if (BUILD_SHARED_LIBS)
        target_compile_definitions(bsdiff_app PRIVATE "BSDIFF_DLL")
    endif()
    target_link_libraries(bsdiff_app PRIVATE bsdiff)

    # bspatch_app
    add_executable(bspatch_app source/bspatch_app.c)
    set_target_properties(bspatch_app PROPERTIES OUTPUT_NAME "bspatch")
    target_include_directories(bspatch_app PRIVATE "include")
    if (BUILD_SHARED_LIBS)
        target_compile_definitions(bspatch_app PRIVATE "BSDIFF_DLL")
    endif()
    target_link_libraries(bspatch_app PRIVATE bsdiff)
endif()

if (BUILD_TESTING)
    add_subdirectory(testdata)
    
    # Configure testing using GoogleTest and GoogleBenchmark
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
    )
    FetchContent_Declare(
      googlebenchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG        v1.8.3
    )
    # Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest googlebenchmark)

    add_subdirectory(test)
endif()
