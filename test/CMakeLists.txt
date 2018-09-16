# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(test_fmt_speed fmt_speed/fmt_speed.cc
                              fmt_speed/stb_sprintf.c
                              ../src/common/kutil.cc
                              fmt_speed/fmt/posix.cc
                              fmt_speed/fmt/format.cc)
target_include_directories(test_fmt_speed PRIVATE fmt_speed)
target_link_libraries(test_fmt_speed PRIVATE miniz)
if(NOT EMSCRIPTEN)
    target_link_libraries(test_fmt_speed PRIVATE Threads::Threads)
endif()
