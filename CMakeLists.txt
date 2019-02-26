# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.1)
project(Rygel C CXX)

# ------------------------------------------------------------------------
# CMake
# ------------------------------------------------------------------------

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake" ${CMAKE_MODULE_PATH})
include("build/cmake/Utility.cmake")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# ------------------------------------------------------------------------
# Compiler settings
# ------------------------------------------------------------------------

find_package(Threads)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -D__STDC_FORMAT_MACROS -Wno-format-zero-length")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -fvisibility=hidden -D__STDC_FORMAT_MACROS -Wno-format-zero-length")

    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS_DEBUG "-O0 -g")
        set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
        set(CMAKE_${lang}_FLAGS_RELEASE "-O2 -DNDEBUG -flto")
    endforeach()

    if(CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
    elseif(MINGW)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields")

        # Latest approach to force MinGW to statically link libwinpthread, until a new
        # version breaks this one too.
        # Number of hours programmers have wasted on dynamic linking: 42547417 (and counting).
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lstdc++ -lwinpthread")
    elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()

    set(STATIC_CRT OFF CACHE BOOL "Link CRT statically")
    if(STATIC_CRT)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()
elseif(MSVC)
    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} /wd4200 /wd4996 -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0")
        set(CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE} /GL")
    endforeach()
    foreach(type EXE SHARED STATIC)
        set(CMAKE_${type}_LINKER_FLAGS_RELEASE "${CMAKE_${type}_LINKER_FLAGS_RELEASE} /LTCG")
    endforeach()

    set(STATIC_CRT ON CACHE BOOL "Link CRT statically")
    if(STATIC_CRT)
        foreach(lang C CXX)
            foreach(mode DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}}")
            endforeach()
        endforeach()
    endif()
endif()

# ------------------------------------------------------------------------
# Platforms definitions
# ------------------------------------------------------------------------

add_definitions(-D_FILE_OFFSET_BITS=64)
if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    if(MSVC)
        add_definitions(-DNOMINMAX)
    elseif(MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    endif()
elseif(APPLE)
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
elseif(LINUX)
    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG} -gsplit-dwarf")
        set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO} -gsplit-dwarf")
    endforeach()
elseif(EMSCRIPTEN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s TOTAL_MEMORY=536870912 -s WASM=1 -s USE_WEBGL2=1 --emrun")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1")
    endif()
endif()

# ------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------

add_subdirectory(lib EXCLUDE_FROM_ALL)
add_subdirectory(src)
