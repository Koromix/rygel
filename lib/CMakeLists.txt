# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
