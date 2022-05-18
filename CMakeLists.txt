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
if(MSVC)
    add_compile_options(/W4 /wd4200 /wd4458 /wd4706 /wd4100 /wd4127 /wd4702)
    enable_language(ASM_MASM)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers
                        -Wno-unused-parameter -Wno-class-memaccess)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-unknown-warning-option)
    endif()
endif()

# ---- Koffi ----

set(KOFFI_SRC
    src/call.cc
    src/abi_arm32.cc
    src/abi_arm64.cc
    src/abi_x64_sysv.cc
    src/abi_x64_win.cc
    src/abi_x86.cc
    src/ffi.cc
    src/parser.cc
    src/util.cc
    vendor/libcc/libcc.cc
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch|arm")
        list(APPEND KOFFI_SRC src/abi_arm64_fwd.S)
    elseif(WIN32)
        list(APPEND KOFFI_SRC src/abi_x64_win_fwd.asm)
    else()
        list(APPEND KOFFI_SRC src/abi_x64_sysv_fwd.S)
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        list(APPEND KOFFI_SRC src/abi_arm32_fwd.S)
    elseif(WIN32)
        list(APPEND KOFFI_SRC src/abi_x86_fwd.asm)
    else()
        list(APPEND KOFFI_SRC src/abi_x86_fwd.S)
    endif()
endif()

add_node_addon(NAME koffi SOURCES ${KOFFI_SRC})
target_include_directories(koffi PRIVATE . vendor/node-addon-api)

target_compile_definitions(koffi PRIVATE FELIX_TARGET=koffi NAPI_DISABLE_CPP_EXCEPTIONS NAPI_VERSION=8)
if(WIN32)
    target_compile_definitions(koffi PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(koffi PRIVATE ws2_32)
endif()
if(NOT MSVC)
    target_compile_options(koffi PRIVATE -fno-exceptions -fno-strict-aliasing)
endif()
