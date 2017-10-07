# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIB_MICROHTTPD_SRC libmicrohttpd/src/microhttpd/base64.c
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
add_library(libmicrohttpd STATIC EXCLUDE_FROM_ALL ${LIB_MICROHTTPD_SRC})
target_include_directories(libmicrohttpd PUBLIC libmicrohttpd libmicrohttpd/src/include)
if(WIN32)
    target_link_libraries(libmicrohttpd PUBLIC ws2_32)
endif()
if(LINUX)
    target_link_libraries(libmicrohttpd PUBLIC pthread)
endif()

set(LIB_LUA_SRC lua/src/lapi.c
                lua/src/lcode.c
                lua/src/lctype.c
                lua/src/ldebug.c
                lua/src/ldo.c
                lua/src/ldump.c
                lua/src/lfunc.c
                lua/src/lgc.c
                lua/src/llex.c
                lua/src/lmem.c
                lua/src/lobject.c
                lua/src/lopcodes.c
                lua/src/lparser.c
                lua/src/lstate.c
                lua/src/lstring.c
                lua/src/ltable.c
                lua/src/ltm.c
                lua/src/lundump.c
                lua/src/lvm.c
                lua/src/lzio.c
                lua/src/lauxlib.c
                lua/src/lbaselib.c
                lua/src/lbitlib.c
                lua/src/lcorolib.c
                lua/src/ldblib.c
                lua/src/liolib.c
                lua/src/lmathlib.c
                lua/src/loslib.c
                lua/src/lstrlib.c
                lua/src/ltablib.c
                lua/src/lutf8lib.c
                lua/src/loadlib.c
                lua/src/linit.c)
add_library(lua STATIC EXCLUDE_FROM_ALL ${LIB_LUA_SRC})
