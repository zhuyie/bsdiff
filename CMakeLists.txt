cmake_minimum_required(VERSION 3.10)

project(bsdiff)

add_executable(bsdiff 
    source/bsdiff.c)
target_link_libraries(bsdiff
    bz2)

add_executable(bspatch 
    source/bspatch.c)
target_link_libraries(bspatch
    bz2)