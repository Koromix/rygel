# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBHEIMDALL_SRC ../common/kutil.cc
                    ../common/kutil.hh
                    libheimdall/core.cc
                    libheimdall/core.hh
                    libheimdall/opengl.cc
                    libheimdall/opengl.hh
                    libheimdall/opengl_func.inc
                    libheimdall/render.cc
                    libheimdall/render.hh
                    libheimdall/runner_win32.cc
                    libheimdall/runner.hh)
add_library(libheimdall STATIC ${LIBHEIMDALL_SRC})
target_link_libraries(libheimdall PUBLIC miniz imgui)
if(WIN32)
    target_link_libraries(libheimdall PUBLIC gdi32 opengl32 imm32)
endif()

set(RUNNER_SRC heimdall/heimdall.cc)
add_executable(heimdall WIN32 ${RUNNER_SRC})
target_link_libraries(heimdall PUBLIC libheimdall)
