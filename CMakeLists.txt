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

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -D__STDC_FORMAT_MACROS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -fvisibility=hidden -D__STDC_FORMAT_MACROS")

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
        set(CMAKE_${lang}_FLAGS "${CMAKE_C_FLAGS} /wd4200 /wd4996 -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0")
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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(lib EXCLUDE_FROM_ALL)
add_subdirectory(test EXCLUDE_FROM_ALL)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow \
-Wno-class-memaccess -Wno-missing-field-initializers -Wno-old-style-cast -Wno-cast-function-type")
endif()

# Libraries and utilities
add_project(src/libdrd EXCLUDE_FROM_ALL)
add_project(src/libgui EXCLUDE_FROM_ALL)
add_project(src/libheimdall EXCLUDE_FROM_ALL)
add_project(src/packer EXCLUDE_FROM_ALL)

# Main projects
add_project(src/drdc)
add_project(src/drdR)
add_project(src/thop)
add_project(src/heimdallR)
add_project(src/heimdallw)

# Misc tools
add_project(tools)
