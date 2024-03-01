# Copyright (C) 2023 Spacemit Co., Ltd.

cmake_minimum_required (VERSION 3.0)
project (v2d-test)

aux_source_directory(dmabufheap DMABUFHEAP)
add_library(dmabufheap SHARED ${DMABUFHEAP})
target_include_directories(dmabufheap PUBLIC dmabufheap)
target_link_libraries(dmabufheap)

aux_source_directory(lib V2D_LIB)
add_library(v2d SHARED ${V2D_LIB})
target_include_directories(v2d PUBLIC inc)
target_link_libraries(v2d)

add_executable(v2d_test v2d_test.c)
target_include_directories(v2d_test PUBLIC inc)
target_link_libraries(v2d_test v2d dmabufheap)

