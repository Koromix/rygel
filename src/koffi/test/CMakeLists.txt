# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the “Software”), to deal in
# the Software without restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
# Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.6)
project(koffi_test C CXX)

find_package(CNoke)

# ---- Vendor libraries ----

include (../cmake/raylib.cmake)
include (../cmake/sqlite3.cmake)

# ---- Test code ----

add_library(sync SHARED sync.c sync2.c)
set_target_properties(sync PROPERTIES PREFIX "")

if(MSVC)
    target_compile_options(sync PRIVATE /wd4116)
    target_link_options(sync PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/sync.def")
endif()

add_library(async SHARED async.c)
set_target_properties(async PROPERTIES PREFIX "")

add_library(callbacks SHARED callbacks.c)
set_target_properties(callbacks PROPERTIES PREFIX "")
target_link_node(callbacks)

add_library(union SHARED union.c)
set_target_properties(union PROPERTIES PREFIX "")

if(WIN32)
    add_library(win32_dep SHARED win32_dep.cc)
    set_target_properties(win32_dep PROPERTIES PREFIX "")

    add_library(win32 SHARED win32.c)
    target_link_libraries(win32 win32_dep)
    set_target_properties(win32 PROPERTIES PREFIX "")
endif()

if(UNIX AND NOT APPLE)
    add_library(posix1 SHARED posix1.c)
    set_target_properties(posix1 PROPERTIES PREFIX "")
    target_compile_options(posix1 PRIVATE "-fsemantic-interposition")

    add_library(posix2 SHARED posix2.c)
    set_target_properties(posix2 PROPERTIES PREFIX "")
    target_compile_options(posix2 PRIVATE "-fsemantic-interposition")

    add_library(posix3 SHARED posix3.c)
    set_target_properties(posix3 PROPERTIES PREFIX "")
    target_compile_options(posix3 PRIVATE "-fsemantic-interposition")
endif()
