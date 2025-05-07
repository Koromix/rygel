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

cmake_minimum_required(VERSION 3.12)
project(koffi_benchmark C CXX)

find_package(CNoke)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)

# ---- Vendor libraries ----

include (../cmake/raylib.cmake)

# ---- atoi ----

add_node_addon(NAME atoi_napi SOURCES atoi_napi.cc ../../../src/core/base/base.cc)
target_include_directories(atoi_napi PRIVATE ../../.. ../../../vendor/node-addon-api)
target_link_libraries(atoi_napi PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(atoi_napi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(atoi_napi PRIVATE ws2_32)
else()
    target_link_libraries(atoi_napi PRIVATE dl)
endif()

# ---- rand ----

add_node_addon(NAME rand_napi SOURCES rand_napi.cc ../../../src/core/base/base.cc)
target_include_directories(rand_napi PRIVATE ../../.. ../../../vendor/node-addon-api)
target_link_libraries(rand_napi PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(rand_napi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(rand_napi PRIVATE ws2_32)
else()
    target_link_libraries(rand_napi PRIVATE dl)
endif()

# ---- Raylib ----

add_executable(raylib_cc raylib_cc.cc  ../../../src/core/base/base.cc)
target_include_directories(raylib_cc PRIVATE ../../..)
target_link_libraries(raylib_cc PRIVATE Threads::Threads raylib)

if(WIN32)
    target_compile_definitions(raylib_cc PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(raylib_cc PRIVATE ws2_32)
else()
    target_link_libraries(raylib_cc PRIVATE dl)
endif()
