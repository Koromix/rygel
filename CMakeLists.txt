cmake_minimum_required(VERSION 3.1)
project(Moya C CXX)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields -fvisibility=hidden -fno-rtti -fno-exceptions \
                                        -Wall -Wextra -Wno-missing-field-initializers -Wno-old-style-cast")
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -gdwarf-2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -gdwarf-2 -DNDEBUG")

set(CMAKE_CXX_STANDARD 17)
if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
endif()
if(MINGW)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
endif()
add_definitions(-D_FILE_OFFSET_BITS=64)

set(LIB_LUA_SRC lib/lua/src/lapi.c
                lib/lua/src/lcode.c
                lib/lua/src/lctype.c
                lib/lua/src/ldebug.c
                lib/lua/src/ldo.c
                lib/lua/src/ldump.c
                lib/lua/src/lfunc.c
                lib/lua/src/lgc.c
                lib/lua/src/llex.c
                lib/lua/src/lmem.c
                lib/lua/src/lobject.c
                lib/lua/src/lopcodes.c
                lib/lua/src/lparser.c
                lib/lua/src/lstate.c
                lib/lua/src/lstring.c
                lib/lua/src/ltable.c
                lib/lua/src/ltm.c
                lib/lua/src/lundump.c
                lib/lua/src/lvm.c
                lib/lua/src/lzio.c
                lib/lua/src/lauxlib.c
                lib/lua/src/lbaselib.c
                lib/lua/src/lbitlib.c
                lib/lua/src/lcorolib.c
                lib/lua/src/ldblib.c
                lib/lua/src/liolib.c
                lib/lua/src/lmathlib.c
                lib/lua/src/loslib.c
                lib/lua/src/lstrlib.c
                lib/lua/src/ltablib.c
                lib/lua/src/lutf8lib.c
                lib/lua/src/loadlib.c
                lib/lua/src/linit.c)
set(MOYA_SRC src/algorithm.cc
             src/algorithm.hh
             src/dump.cc
             src/dump.hh
             src/moya.cc
             src/kutil.cc
             src/kutil.hh
             src/stays.cc
             src/stays.hh
             src/tables.cc
             src/tables.hh)

add_executable(moya ${MOYA_SRC})
