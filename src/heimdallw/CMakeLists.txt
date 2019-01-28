# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT TARGET heimdall)
    return()
endif()

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX .html)
endif()

set(HEIMDALLW_SRC
    heimdallw.cc
)

add_executable(heimdallw ${HEIMDALLW_SRC})
target_link_libraries(heimdallw PRIVATE heimdall)
