# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

cmake_minimum_required(VERSION 3.12)
project(koffi C CXX ASM)

find_package(CNoke)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(NOT TARGET koffi)
    add_subdirectory(.. koffi)
endif()
add_subdirectory(../test test)

# ---- atoi ----

add_executable(atoi_cc atoi_cc.cc ../vendor/libcc/libcc.cc)
target_include_directories(atoi_cc PRIVATE ..)
target_link_libraries(atoi_cc PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(atoi_cc PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(atoi_cc PRIVATE ws2_32)
endif()

add_node_addon(NAME atoi_napi SOURCES atoi_napi.cc ../vendor/libcc/libcc.cc)
target_include_directories(atoi_napi PRIVATE .. ../vendor/node-addon-api)
target_link_libraries(atoi_napi PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(atoi_napi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(atoi_napi PRIVATE ws2_32)
endif()

# ---- Raylib ----

add_executable(raylib_cc atoi_cc.cc ../vendor/libcc/libcc.cc)
target_include_directories(raylib_cc PRIVATE ..)
target_link_libraries(raylib_cc PRIVATE Threads::Threads)
add_dependencies(raylib_cc raylib)

if(WIN32)
    target_compile_definitions(raylib_cc PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(raylib_cc PRIVATE ws2_32)
endif()
