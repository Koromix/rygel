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

cmake_minimum_required(VERSION 3.11)
project(koffi C CXX ASM)

find_package(CNoke)

set(CMAKE_CXX_STANDARD 17)
if(MSVC)
    enable_language(ASM_MASM)
endif()

# ---- Koffi ----

set(KOFFI_SRC
    src/call_arm32.cc
    src/call_arm64.cc
    src/call_x64_sysv.cc
    src/call_x64_win.cc
    src/call_x86.cc
    src/ffi.cc
    src/util.cc
    vendor/libcc/libcc.cc
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(WIN32)
        list(APPEND KOFFI_SRC src/call_x64_win_fwd.asm)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
        list(APPEND KOFFI_SRC src/call_arm64_fwd.S)
    else()
        list(APPEND KOFFI_SRC src/call_x64_sysv_fwd.S)
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    if(WIN32)
        list(APPEND KOFFI_SRC src/call_x86_fwd.asm)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3456]86|x86|AMD64")
        list(APPEND KOFFI_SRC src/call_x86_fwd.S)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv[678]l")
        list(APPEND KOFFI_SRC src/call_arm32_fwd.S)
    endif()
endif()

add_node_addon(NAME koffi SOURCES ${KOFFI_SRC})
target_include_directories(koffi PRIVATE . vendor/node-addon-api)

target_compile_definitions(koffi PRIVATE FELIX_TARGET=koffi NAPI_DISABLE_CPP_EXCEPTIONS)
if(WIN32)
    target_compile_definitions(koffi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(koffi PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(koffi PRIVATE /wd4200)
else()
    target_compile_options(koffi PRIVATE -fno-exceptions -Wno-missing-field-initializers
                                         -fno-strict-aliasing)
endif()

# ---- Raylib ----

add_library(raylib SHARED
    vendor/raylib/src/rcore.c
    vendor/raylib/src/rshapes.c
    vendor/raylib/src/rtextures.c
    vendor/raylib/src/rtext.c
    vendor/raylib/src/rmodels.c
    vendor/raylib/src/utils.c
    vendor/raylib/src/rglfw.c
    vendor/raylib/src/raudio.c
)
set_target_properties(raylib PROPERTIES PREFIX "")
target_include_directories(raylib PRIVATE vendor/raylib/src/external/glfw/include)
target_compile_definitions(raylib PRIVATE PLATFORM_DESKTOP GRAPHICS_API_OPENGL_21
                                          BUILD_LIBTYPE_SHARED NDEBUG)

if(WIN32)
    target_compile_definitions(raylib PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(raylib PRIVATE winmm)
endif()
if(MSVC)
    target_compile_options(raylib PRIVATE /wd4244 /wd4305)
else()
    target_compile_options(raylib PRIVATE -Wno-sign-compare -Wno-old-style-declaration
                                          -Wno-unused-function -Wno-missing-field-initializers
                                          -Wno-unused-value -Wno-implicit-fallthrough -Wno-stringop-overflow
                                          -Wno-unused-result)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "BSD")
    target_include_directories(raylib PRIVATE /usr/local/include)
endif()

# ---- SQLite ----

add_library(sqlite3 SHARED vendor/sqlite3/sqlite3.c)
set_target_properties(sqlite3 PROPERTIES PREFIX "")

if(WIN32)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_API=__declspec\(dllexport\))
endif()

# ---- Misc ----

add_library(misc SHARED test/tests/misc.c)
set_target_properties(misc PROPERTIES PREFIX "")
