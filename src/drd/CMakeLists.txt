# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(DRD_SRC ../common/kutil.cc
            ../common/kutil.hh
            libdrd/a_classifier.cc
            libdrd/a_classifier.hh
            libdrd/a_constraints.cc
            libdrd/a_constraints.hh
            libdrd/d_authorizations.cc
            libdrd/d_authorizations.hh
            libdrd/d_codes.hh
            libdrd/d_desc.cc
            libdrd/d_desc.hh
            libdrd/d_prices.cc
            libdrd/d_prices.hh
            libdrd/d_stays.cc
            libdrd/d_stays.hh
            libdrd/d_tables.cc
            libdrd/d_tables.hh
            libdrd/main.cc
            libdrd/main.hh)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(drd SHARED ${DRD_SRC})
else()
    add_library(drd STATIC ${DRD_SRC})
endif()
target_link_libraries(drd PUBLIC miniz)
add_amalgamated_file(drd "${CMAKE_BINARY_DIR}/libdrd.hh" libdrd/libdrd.hh)

set(DRDC_SRC drdc/drd.cc
             drdc/dump.cc
             drdc/dump.hh)
add_executable(drdc ${DRDC_SRC})
target_link_libraries(drdc PUBLIC drd)

# Application HTML must be kept first
set(DRDW_RESOURCES drdw/static/drdw.html
                   drdw/static/drdw.css
                   drdw/static/drdw.js
                   drdw/static/logo.png
                   ../../lib/chartjs/chart.min.js)
set(DRDW_SRC drdw/drdw.cc
             drdw/resources.hh)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_drdw_resources.cmake" [=[
set(RESOURCES_CC_FILENAME "${BIN_DIR}/drdw/resources.cc")

file(WRITE ${RESOURCES_CC_FILENAME} "\
#include \"${SRC_DIR}/libdrd/libdrd.hh\"\n\
#include \"${SRC_DIR}/drdw/resources.hh\"\n\
\n\
EXPORT extern const Span<const Resource> static_resources;
const Span<const Resource> static_resources = {\n")
separate_arguments(RESOURCES)
foreach(res ${RESOURCES})
    get_filename_component(name ${res} NAME)

    file(READ "${SRC_DIR}/${res}" data HEX)
    string(REGEX REPLACE "([a-fA-F0-9][a-fA-F0-9])" "0x\\1," data ${data})

    file(APPEND ${RESOURCES_CC_FILENAME} "    {\"/static/${name}\", {${data}}},\n")
endforeach()
file(APPEND ${RESOURCES_CC_FILENAME} "};\n")
]=])
add_custom_command(
    OUTPUT drdw/resources.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DRESOURCES="${DRDW_RESOURCES}"
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_drdw_resources.cmake"
    DEPENDS ${DRDW_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    add_library(drdw_res SHARED "${CMAKE_CURRENT_BINARY_DIR}/drdw/resources.cc")
    set_target_properties(drdw_res PROPERTIES PREFIX "")
else()
    list(APPEND DRDW_SRC "${CMAKE_CURRENT_BINARY_DIR}/drdw/resources.cc")
endif()
add_executable(drdw ${DRDW_SRC} ${DRDW_RESOURCES})
target_include_directories(drdw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/drdw")
target_link_libraries(drdw PUBLIC drd libmicrohttpd)
