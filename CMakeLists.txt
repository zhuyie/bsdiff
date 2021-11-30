cmake_minimum_required(VERSION 3.10)

project(bsdiff)

add_executable(bsdiff 
    source/bsdiff.c
    3rdparty/bzip2/bzlib.c
    3rdparty/bzip2/compress.c
    3rdparty/bzip2/decompress.c
    3rdparty/bzip2/blocksort.c
    3rdparty/bzip2/crctable.c
    3rdparty/bzip2/huffman.c
    3rdparty/bzip2/randtable.c)
target_include_directories(bsdiff
    PUBLIC "3rdparty/bzip2")

add_executable(bspatch 
    source/bspatch.c
    3rdparty/bzip2/bzlib.c
    3rdparty/bzip2/compress.c
    3rdparty/bzip2/decompress.c
    3rdparty/bzip2/blocksort.c
    3rdparty/bzip2/crctable.c
    3rdparty/bzip2/huffman.c
    3rdparty/bzip2/randtable.c)
target_include_directories(bspatch
    PUBLIC "3rdparty/bzip2")
