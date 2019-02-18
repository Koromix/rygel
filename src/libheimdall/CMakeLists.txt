# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT TARGET libgui)
    return()
endif()

set(LIBHEIMDALL_RESOURCES
    ../../lib/imgui/misc/fonts/Roboto-Medium.ttf
)
set(LIBHEIMDALL_SRC
    ../wrappers/opengl.cc
    ../wrappers/opengl.hh
    ../wrappers/opengl_func.inc
    animation.hh
    core.cc
    core.hh
    data.hh
    render.cc
    render.hh
)

add_custom_command(OUTPUT assets.cc
                   COMMAND $<TARGET_FILE:packer>
                   ARGS -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${LIBHEIMDALL_RESOURCES}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   DEPENDS packer ${LIBHEIMDALL_RESOURCES})
list(APPEND LIBHEIMDALL_SRC "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")

add_library(libheimdall ${LIBHEIMDALL_SRC})
set_target_properties(libheimdall PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libheimdall PUBLIC libcc imgui libgui)
add_amalgamated_file(libheimdall "${CMAKE_BINARY_DIR}/libheimdall.cc" libheimdall.cc)
add_amalgamated_file(libheimdall "${CMAKE_BINARY_DIR}/libheimdall.hh" libheimdall.hh)
