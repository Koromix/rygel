# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

cmake_minimum_required(VERSION 3.12)
project(koffi_benchmark C CXX)

find_package(CNoke)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)

# ---- Vendor libraries ----

include (../cmake/raylib.cmake)

# ---- atoi ----

add_node_addon(NAME atoi_napi SOURCES atoi_napi.cc ../../../lib/native/base/base.cc)
target_include_directories(atoi_napi PRIVATE ../../.. ../../../vendor/node-addon-api)
target_link_libraries(atoi_napi PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(atoi_napi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(atoi_napi PRIVATE ws2_32)
else()
    target_link_libraries(atoi_napi PRIVATE dl)
endif()

# ---- rand ----

add_node_addon(NAME rand_napi SOURCES rand_napi.cc ../../../lib/native/base/base.cc)
target_include_directories(rand_napi PRIVATE ../../.. ../../../vendor/node-addon-api)
target_link_libraries(rand_napi PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(rand_napi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(rand_napi PRIVATE ws2_32)
else()
    target_link_libraries(rand_napi PRIVATE dl)
endif()

# ---- Raylib ----

add_executable(raylib_cc raylib_cc.cc  ../../../lib/native/base/base.cc)
target_include_directories(raylib_cc PRIVATE ../../..)
target_link_libraries(raylib_cc PRIVATE Threads::Threads raylib)

if(WIN32)
    target_compile_definitions(raylib_cc PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(raylib_cc PRIVATE ws2_32)
else()
    target_link_libraries(raylib_cc PRIVATE dl)
endif()
