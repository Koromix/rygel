# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The platform layer is only available for Win32, for now
if (NOT WIN32)
    return()
endif()

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
                    runner_win32.cc
                    runner.hh)
add_library(heimdall ${LIBHEIMDALL_SRC})
set_target_properties(heimdall PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(heimdall PUBLIC miniz imgui Threads::Threads)
if(WIN32)
    target_link_libraries(heimdall PUBLIC gdi32 opengl32 imm32)
endif()
add_amalgamated_file(heimdall "${CMAKE_BINARY_DIR}/libheimdall.hh" libheimdall.hh)
