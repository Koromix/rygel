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

cmake_minimum_required(VERSION 3.6)
project(koffi C CXX ASM)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.9.0")
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT USE_LTO)
endif()

find_package(CNoke)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.8.0")
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 14)
endif()
if(MSVC)
    add_compile_options(/Zc:__cplusplus /W4 /wd4200 /wd4458 /wd4706 /wd4100 /wd4127 /wd4702 /wd4201 /wd4324)

    # ASM_MASM does not (yet) work on Windows ARM64
    if(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        enable_language(ASM_MASM)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-unknown-warning-option)
    endif()
endif()

# ---- Koffi ----

set(KOFFI_SRC
    src/call.cc
    src/ffi.cc
    src/parser.cc
    src/util.cc
    vendor/libcc/libcc.cc
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # CMAKE_SYSTEM_PROCESSOR is wrong on Windows ARM64

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch|arm" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        if(WIN32)
            get_filename_component(cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            file(TO_CMAKE_PATH "${cl_dir}/armasm64.exe" asm_compiler)

            # Work around missing ARM64-native ARMASM64 compiler (at least in VS 17.3 Preview 2)
            if(NOT EXISTS "${asm_compiler}")
                file(TO_CMAKE_PATH "${cl_dir}/../../Hostx64/arm64/armasm64.exe" asm_compiler)
            endif()

            message(STATUS "Using ARMASM64 compiler: ${asm_compiler}")

            file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/abi_arm64_fwd.asm" asm_source)
            file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/abi_arm64_fwd.obj" asm_object)

            add_custom_command(
                OUTPUT "${asm_object}"
                COMMAND "${asm_compiler}" ARGS /nologo /o "${asm_object}" "${asm_source}"
                DEPENDS "${asm_source}"
                COMMENT "Assembling ${asm_source}"
            )
            set_source_files_properties("${asm_object}" PROPERTIES EXTERNAL_OBJECT TRUE)

            list(APPEND KOFFI_SRC src/abi_arm64.cc "${asm_object}")
        else()
            list(APPEND KOFFI_SRC src/abi_arm64.cc src/abi_arm64_fwd.S)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv")
        list(APPEND KOFFI_SRC src/abi_riscv64.cc src/abi_riscv64_fwd.S)
    else()
        if(WIN32)
            list(APPEND KOFFI_SRC src/abi_x64_win.cc src/abi_x64_win_fwd.asm)
        else()
            list(APPEND KOFFI_SRC src/abi_x64_sysv.cc src/abi_x64_sysv_fwd.S)
        endif()
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        list(APPEND KOFFI_SRC src/abi_arm32.cc src/abi_arm32_fwd.S)
    else()
        if(WIN32)
            list(APPEND KOFFI_SRC src/abi_x86.cc src/abi_x86_fwd.asm)
        else()
            list(APPEND KOFFI_SRC src/abi_x86.cc src/abi_x86_fwd.S)
        endif()
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
    # Restore C/C++ compiler sanity
    target_compile_options(koffi PRIVATE -fno-exceptions -fno-strict-aliasing -fwrapv
                                         -fno-delete-null-pointer-checks -fno-finite-loops)
endif()
if(USE_LTO)
    set_target_properties(koffi PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
