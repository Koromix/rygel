# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The platform layer is only available for Win32, for now

set(GUI_SRC libgui.cc
            libgui.hh
            ../common/opengl.cc
            ../common/opengl.hh
            ../common/opengl_func.inc)
if (WIN32)
    list(APPEND GUI_SRC runner_win32.cc)
elseif(EMSCRIPTEN)
    list(APPEND GUI_SRC runner_emsdk.cc)
else()
    return()
endif()

add_library(gui ${GUI_SRC})
set_target_properties(gui PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(gui PUBLIC miniz)
if(WIN32)
    target_link_libraries(gui PUBLIC gdi32 opengl32 imm32)
endif()
if(NOT EMSCRIPTEN)
    target_link_libraries(gui PUBLIC Threads::Threads)
endif()
add_amalgamated_file(gui "${CMAKE_BINARY_DIR}/libgui.cc" libgui.cc)
add_amalgamated_file(gui "${CMAKE_BINARY_DIR}/libgui.hh" libgui.hh)
