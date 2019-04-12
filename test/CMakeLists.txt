# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow \
-Wno-class-memaccess -Wno-missing-field-initializers -Wno-old-style-cast -Wno-cast-function-type")
endif()

# ------------------------------------------------------------------------
# bench_libcc
# ------------------------------------------------------------------------

add_executable(bench_libcc
    bench_libcc/vendor/stb_sprintf.c
    bench_libcc/vendor/fmt/posix.cc
    bench_libcc/vendor/fmt/format.cc

    bench_libcc/bench_libcc.cc
    bench_libcc/bench_libcc.hh
    bench_libcc/bench_fmt.cc
)
target_include_directories(bench_libcc PRIVATE bench_libcc/vendor)
target_link_libraries(bench_libcc PRIVATE libcc)
