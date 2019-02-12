# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The platform layer is only available for Win32, for now

set(LIBGUI_SRC
    ../wrappers/opengl.cc
    ../wrappers/opengl.hh
    ../wrappers/opengl_func.inc
    libgui.cc
    libgui.hh
)
if (WIN32)
    list(APPEND LIBGUI_SRC runner_win32.cc)
elseif(EMSCRIPTEN)
    list(APPEND LIBGUI_SRC runner_emsdk.cc)
else()
    return()
endif()

add_library(gui ${LIBGUI_SRC})
set_target_properties(gui PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(gui PUBLIC cc)
if(WIN32)
    target_link_libraries(gui PUBLIC gdi32 opengl32 imm32)
endif()
add_amalgamated_file(gui "${CMAKE_BINARY_DIR}/libgui.cc" libgui.cc)
add_amalgamated_file(gui "${CMAKE_BINARY_DIR}/libgui.hh" libgui.hh)
