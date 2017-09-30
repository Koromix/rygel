# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(CORE_SRC core/algorithm.cc
             core/algorithm.hh
             core/codes.hh
             core/kutil.cc
             core/kutil.hh
             core/pricing.cc
             core/pricing.hh
             core/stays.cc
             core/stays.hh
             core/tables.cc
             core/tables.hh)
add_library(moya STATIC ${CORE_SRC})
add_amalgamated_file(moya "${CMAKE_BINARY_DIR}/libmoya.hh" core/libmoya.hh)

set(CLI_SRC cli/dump.cc
            cli/dump.hh
            cli/moya_cli.cc)
add_executable(moya_cli ${CLI_SRC})
set_target_properties(moya_cli PROPERTIES OUTPUT_NAME moya)
target_link_libraries(moya_cli PUBLIC moya)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_web_pages.cmake" [=[
set(PAGES_CC_FILENAME "${BIN_DIR}/web/pages.cc")
file(WRITE ${PAGES_CC_FILENAME} "\
#include \"${SRC_DIR}/core/libmoya.hh\"\n\
#include \"${SRC_DIR}/web/pages.hh\"\n")
foreach(page ${PAGES})
    get_filename_component(var ${page} NAME_WE)
    set(var "page_${var}")
    file(READ "${SRC_DIR}/${page}" html)
    file(APPEND ${PAGES_CC_FILENAME}
         "\nconst ArrayRef<const char> ${var} = MakeStrRef(R\"html(${html})html\");\n")
endforeach()
]=])

set(WEB_PAGES web/index.html)
add_custom_command(
    OUTPUT web/pages.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DPAGES=${WEB_PAGES}
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_web_pages.cmake"
    DEPENDS ${WEB_PAGES})

set(WEB_SRC web/talyn.cc
            ${CMAKE_CURRENT_BINARY_DIR}/web/pages.cc
            web/pages.hh)
add_executable(talyn ${WEB_SRC} ${WEB_PAGES})
target_link_libraries(talyn PUBLIC moya libmicrohttpd)
