# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBHEIMDALL_SRC ../common/kutil.cc
                    ../common/kutil.hh
                    core.cc
                    core.hh
                    data.cc
                    data.hh
                    opengl.cc
                    opengl.hh
                    opengl_func.inc
                    render.cc
                    render.hh
                    runner_win32.cc
                    runner.hh)
add_library(libheimdall STATIC ${LIBHEIMDALL_SRC})
set_target_properties(libheimdall PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libheimdall PUBLIC miniz imgui)
if(WIN32)
    target_link_libraries(libheimdall PUBLIC gdi32 opengl32 imm32)
endif()
add_amalgamated_file(libheimdall "${CMAKE_BINARY_DIR}/libheimdall.hh" libheimdall.hh)
