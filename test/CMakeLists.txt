# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wno-strict-overflow \
-Wno-class-memaccess -Wno-missing-field-initializers -Wno-old-style-cast -Wno-cast-function-type")
endif()

# ------------------------------------------------------------------------
# libcc_bench
# ------------------------------------------------------------------------

add_executable(libcc_bench
    libcc_bench/vendor/stb_sprintf.c
    libcc_bench/vendor/fmt/posix.cc
    libcc_bench/vendor/fmt/format.cc

    libcc_bench/bench.cc
    libcc_bench/bench.hh
    libcc_bench/bench_fmt.cc
)
target_include_directories(libcc_bench PRIVATE libcc_bench/vendor)
target_link_libraries(libcc_bench PRIVATE libcc)
