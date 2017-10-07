# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(MOYA_SRC moya/algorithm.cc
             moya/algorithm.hh
             moya/codes.hh
             moya/kutil.cc
             moya/kutil.hh
             moya/pricing.cc
             moya/pricing.hh
             moya/stays.cc
             moya/stays.hh
             moya/tables.cc
             moya/tables.hh)
add_library(moya STATIC ${MOYA_SRC})
add_amalgamated_file(moya "${CMAKE_BINARY_DIR}/libmoya.hh" moya/libmoya.hh)

set(DRD_SRC drd/dump.cc
            drd/dump.hh
            drd/drd.cc)
add_executable(drd ${DRD_SRC})
target_link_libraries(drd PUBLIC moya)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_pages.cmake" [=[
set(PAGES_CC_FILENAME "${BIN_DIR}/talyn/pages.cc")
file(WRITE ${PAGES_CC_FILENAME} "\
#include \"${SRC_DIR}/moya/libmoya.hh\"\n\
#include \"${SRC_DIR}/talyn/pages.hh\"\n")
foreach(page ${PAGES})
    get_filename_component(var ${page} NAME_WE)
    set(var "page_${var}")
    file(READ "${SRC_DIR}/${page}" html)
    file(APPEND ${PAGES_CC_FILENAME}
         "\nconst ArrayRef<const char> ${var} = MakeStrRef(R\"html(${html})html\");\n")
endforeach()
]=])

set(TALYN_PAGES talyn/index.html)
set(TALYN_SRC talyn/talyn.cc
              ${CMAKE_CURRENT_BINARY_DIR}/talyn/pages.cc
              talyn/pages.hh)
add_custom_command(
    OUTPUT talyn/pages.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DPAGES=${TALYN_PAGES}
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_pages.cmake"
    DEPENDS ${TALYN_PAGES})
add_executable(talyn ${TALYN_SRC} ${TALYN_PAGES})
target_link_libraries(talyn PUBLIC moya libmicrohttpd)
