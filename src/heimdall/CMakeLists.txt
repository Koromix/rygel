# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(HEIMDALL_SRC libheimdall/core.cc
                 libheimdall/core.hh
                 ../common/kutil.cc
                 ../common/kutil.hh
                 libheimdall/opengl.cc
                 libheimdall/opengl.hh
                 libheimdall/opengl_func.inc
                 libheimdall/render.cc
                 libheimdall/render.hh)
add_library(heimdall STATIC ${HEIMDALL_SRC})
target_link_libraries(heimdall PUBLIC miniz imgui gdi32 opengl32 imm32)

set(RUNNER_SRC runner/main.cc
               runner/runner_win32.cc
               runner/runner.hh)
add_executable(runner WIN32 ${RUNNER_SRC})
set_target_properties(runner PROPERTIES OUTPUT_NAME heimdall)
target_link_libraries(runner PUBLIC heimdall)
