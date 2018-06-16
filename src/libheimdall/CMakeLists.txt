# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The platform layer is only available for Win32, for now

set(LIBHEIMDALL_RESOURCES ../../lib/imgui/misc/fonts/Roboto-Medium.ttf)
set(LIBHEIMDALL_SRC ../common/kutil.cc
                    ../common/kutil.hh
                    animation.hh
                    core.cc
                    core.hh
                    data.hh
                    opengl.cc
                    opengl.hh
                    opengl_func.inc
                    render.cc
                    render.hh
                    runner.hh)
if (WIN32)
    list(APPEND LIBHEIMDALL_SRC runner_win32.cc)
elseif(EMSCRIPTEN)
    list(APPEND LIBHEIMDALL_SRC runner_emsdk.cc)
else()
    return()
endif()

add_custom_command(OUTPUT assets.cc
                   COMMAND $<TARGET_FILE:packer>
                   ARGS -cnone -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${LIBHEIMDALL_RESOURCES}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   DEPENDS packer ${LIBHEIMDALL_RESOURCES})
list(APPEND LIBHEIMDALL_SRC "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")

add_library(heimdall ${LIBHEIMDALL_SRC})
set_target_properties(heimdall PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(heimdall PUBLIC miniz imgui)
if(WIN32)
    target_link_libraries(heimdall PUBLIC gdi32 opengl32 imm32)
endif()
if(NOT EMSCRIPTEN)
    target_link_libraries(heimdall PUBLIC Threads::Threads)
endif()
add_amalgamated_file(heimdall "${CMAKE_BINARY_DIR}/libheimdall.cc" libheimdall.cc)
add_amalgamated_file(heimdall "${CMAKE_BINARY_DIR}/libheimdall.hh" libheimdall.hh)
