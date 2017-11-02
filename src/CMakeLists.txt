# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(MOYA_SRC moya/algorithm.cc
             moya/algorithm.hh
             moya/constraints.cc
             moya/constraints.hh
             moya/codes.hh
             moya/data.cc
             moya/data.hh
             moya/kutil.cc
             moya/kutil.hh
             moya/main.cc
             moya/main.hh
             moya/pricing.cc
             moya/pricing.hh
             moya/tables.cc
             moya/tables.hh)
add_library(moya STATIC ${MOYA_SRC})
add_amalgamated_file(moya "${CMAKE_BINARY_DIR}/libmoya.hh" moya/libmoya.hh)

set(DRD_SRC drd/dump.cc
            drd/dump.hh
            drd/drd.cc)
add_executable(drd ${DRD_SRC})
target_link_libraries(drd PUBLIC moya)

set(TALYN_RESOURCES talyn/talyn.html
                    talyn/talyn.css
                    talyn/talyn.js
                    ../resources/logo.png
                    ../lib/chartjs/chart.min.js)
set(TALYN_SRC talyn/talyn.cc
              ${CMAKE_CURRENT_BINARY_DIR}/talyn/resources.cc)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_resources.cmake" [=[
set(RESOURCES_CC_FILENAME "${BIN_DIR}/talyn/resources.cc")
set(RESOURCES_HH_FILENAME "${BIN_DIR}/talyn/resources.hh")

file(WRITE ${RESOURCES_CC_FILENAME} "\
#include \"${SRC_DIR}/moya/libmoya.hh\"\n\
#include \"resources.hh\"\n")
file(WRITE ${RESOURCES_HH_FILENAME} "\
#include \"${SRC_DIR}/moya/libmoya.hh\"\n\n\
namespace resources {\n\n")

separate_arguments(RESOURCES)
foreach(res ${RESOURCES})
    get_filename_component(var ${res} NAME)
    string(MAKE_C_IDENTIFIER ${var} var)
    file(READ "${SRC_DIR}/${res}" data HEX)
    string(REGEX REPLACE "([a-fA-F0-9][a-fA-F0-9])" "0x\\1," data ${data})
    file(APPEND ${RESOURCES_CC_FILENAME}
         "\nconst ArrayRef<const uint8_t> resources::${var} = {${data}};\n")
    file(APPEND ${RESOURCES_HH_FILENAME}
        "extern const ArrayRef<const uint8_t> ${var};\n")
endforeach()

file(APPEND ${RESOURCES_HH_FILENAME} "\n}")
]=])
add_custom_command(
    OUTPUT talyn/resources.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DRESOURCES="${TALYN_RESOURCES}"
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_resources.cmake"
    DEPENDS ${TALYN_RESOURCES})
add_executable(talyn ${TALYN_SRC} ${TALYN_RESOURCES})
target_include_directories(talyn PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/talyn")
target_link_libraries(talyn PUBLIC moya libmicrohttpd)
