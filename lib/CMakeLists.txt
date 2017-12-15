# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LIBMICROHTTPD_SRC libmicrohttpd/src/microhttpd/base64.c
                      libmicrohttpd/src/microhttpd/basicauth.c
                      libmicrohttpd/src/microhttpd/connection.c
                      # libmicrohttpd/src/microhttpd/connection_https.c
                      libmicrohttpd/src/microhttpd/daemon.c
                      libmicrohttpd/src/microhttpd/digestauth.c
                      libmicrohttpd/src/microhttpd/internal.c
                      libmicrohttpd/src/microhttpd/md5.c
                      libmicrohttpd/src/microhttpd/memorypool.c
                      libmicrohttpd/src/microhttpd/mhd_compat.c
                      libmicrohttpd/src/microhttpd/mhd_itc.c
                      libmicrohttpd/src/microhttpd/mhd_mono_clock.c
                      libmicrohttpd/src/microhttpd/mhd_sockets.c
                      libmicrohttpd/src/microhttpd/mhd_str.c
                      libmicrohttpd/src/microhttpd/mhd_threads.c
                      libmicrohttpd/src/microhttpd/postprocessor.c
                      libmicrohttpd/src/microhttpd/reason_phrase.c
                      libmicrohttpd/src/microhttpd/response.c
                      libmicrohttpd/src/microhttpd/sysfdsetsize.c
                      libmicrohttpd/src/microhttpd/tsearch.c)
add_library(libmicrohttpd STATIC EXCLUDE_FROM_ALL ${LIBMICROHTTPD_SRC})
target_include_directories(libmicrohttpd PRIVATE libmicrohttpd libmicrohttpd/src/include)
if(WIN32)
    target_link_libraries(libmicrohttpd PUBLIC ws2_32)
endif()
if(LINUX)
    target_link_libraries(libmicrohttpd PUBLIC pthread)
endif()

set(MINIZ_SRC miniz/miniz.c)
add_library(miniz STATIC EXCLUDE_FROM_ALL ${MINIZ_SRC})
target_compile_definitions(miniz PRIVATE -DMINIZ_NO_STDIO
                                         -DMINIZ_NO_TIME
                                         -DMINIZ_NO_ARCHIVE_APIS
                                         -DMINIZ_NO_ARCHIVE_WRITING_APIS
                                         -DMINIZ_NO_ZLIB_APIS
                                         -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES)

set(IMGUI_SRC imgui/imgui.cpp
              imgui/imgui_draw.cpp
              imgui/imgui_demo.cpp)
add_library(imgui STATIC EXCLUDE_FROM_ALL ${IMGUI_SRC})
