# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.1)
project(Rygel C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake" ${CMAKE_MODULE_PATH})
include("build/cmake/Utility.cmake")
find_package(Threads)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_CXX_STANDARD 14)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields -fno-rtti -fno-exceptions -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

    if(MINGW)
        # Latest approach to force MinGW to statically link libwinpthread, until a new
        # version breaks this one too.
        # Number of hours programmers have wasted on dynamic linking: 42547417 (and counting).
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lstdc++ -lwinpthread")
    endif()
endif()

add_definitions(-D_FILE_OFFSET_BITS=64)
if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    if(MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    endif()
elseif(EMSCRIPTEN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s TOTAL_MEMORY=1073741824 -s OUTLINING_LIMIT=160000 -s WASM=1")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(lib)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wsign-conversion -Wconversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wsign-conversion -Wconversion \
                                            -Wno-missing-field-initializers -Wno-old-style-cast -Wno-strict-overflow")
endif()

add_projects("src/lib*" EXCLUDE_FROM_ALL)
add_projects("src/*")
