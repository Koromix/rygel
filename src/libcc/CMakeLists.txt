# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBCC_SRC
    ../libcc/libcc.cc
    ../libcc/libcc.hh
)

add_library(libcc ${LIBCC_SRC})
set_target_properties(libcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libcc PUBLIC miniz)
if(NOT EMSCRIPTEN)
    target_link_libraries(libcc PUBLIC Threads::Threads)
endif()
