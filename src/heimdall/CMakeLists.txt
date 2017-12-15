cmake_minimum_required(VERSION 3.1)
project(Heimdall C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include("build/cmake/Utility.cmake")

set(CMAKE_CXX_STANDARD 14)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields -fno-rtti -fno-exceptions")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -gdwarf-2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -gdwarf-2 -DNDEBUG")

    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    endif()
endif()

add_definitions(-D_FILE_OFFSET_BITS=64)
if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
    if(MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(lib)
add_subdirectory(src)
