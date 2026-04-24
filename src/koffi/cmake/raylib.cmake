# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

add_library(raylib SHARED
    ../../../vendor/raylib/src/rcore.c
    ../../../vendor/raylib/src/rshapes.c
    ../../../vendor/raylib/src/rtextures.c
    ../../../vendor/raylib/src/rtext.c
    ../../../vendor/raylib/src/rmodels.c
)
set_target_properties(raylib PROPERTIES PREFIX "")
target_include_directories(raylib PRIVATE ../../../vendor/raylib/src/external/glfw/include)
target_compile_definitions(raylib PRIVATE PLATFORM_MEMORY GRAPHICS_API_OPENGL_SOFTWARE BUILD_LIBTYPE_SHARED NDEBUG)

if(MSVC)
    target_compile_options(raylib PRIVATE /wd4244 /wd4305)
else()
    target_compile_options(raylib PRIVATE -Wno-sign-compare -Wno-old-style-declaration
                                          -Wno-unused-function -Wno-missing-field-initializers
                                          -Wno-unused-value -Wno-implicit-fallthrough -Wno-stringop-overflow
                                          -Wno-unused-result)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(raylib PRIVATE -Wno-unknown-warning-option)
    endif()
endif()

if(WIN32)
    target_compile_definitions(raylib PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_link_libraries(raylib PRIVATE winmm)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(raylib PRIVATE _GNU_SOURCE)
endif()
