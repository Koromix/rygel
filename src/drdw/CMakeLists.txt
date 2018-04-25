# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (EMSCRIPTEN)
    return()
endif()

# Application HTML must be kept first
set(DRDW_RESOURCES static/drdw.html
                   static/drdw.css
                   static/common.js
                   static/favicon.ico
                   static/pricing.js
                   static/casemix.js
                   static/logo.png
                   ../../lib/chartjs/chart.min.js)
set(DRDW_SRC drdw.cc
             resources.hh)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_drdw_resources.cmake" [=[
set(RESOURCES_CC_FILENAME "${BIN_DIR}/resources.cc")

file(WRITE ${RESOURCES_CC_FILENAME} "\
#include \"${SRC_DIR}/../libdrd/libdrd.hh\"\n\
#include \"${SRC_DIR}/resources.hh\"\n\
\n\
static const Resource resources[] = {\n")
separate_arguments(RESOURCES)
foreach(res ${RESOURCES})
    get_filename_component(name ${res} NAME)

    file(READ "${SRC_DIR}/${res}" data HEX)
    string(REGEX REPLACE "([a-fA-F0-9][a-fA-F0-9])" "0x\\1," data ${data})

    file(APPEND ${RESOURCES_CC_FILENAME} "    {\"/static/${name}\", {${data}}},\n")
endforeach()
file(APPEND ${RESOURCES_CC_FILENAME} "};\n\
\n\
EXPORT extern const Span<const Resource> static_resources;\n\
const Span<const Resource> static_resources = resources;\n")
]=])
add_custom_command(
    OUTPUT resources.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DRESOURCES="${DRDW_RESOURCES}"
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_drdw_resources.cmake"
    DEPENDS ${DRDW_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(drdw_res MODULE "${CMAKE_CURRENT_BINARY_DIR}/resources.cc")
    set_target_properties(drdw_res PROPERTIES PREFIX "")
else()
    list(APPEND DRDW_SRC "${CMAKE_CURRENT_BINARY_DIR}/resources.cc")
endif()

add_executable(drdw ${DRDW_SRC} ${DRDW_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
    target_link_libraries(drdw PRIVATE dl)
endif()
target_include_directories(drdw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(drdw PUBLIC drd microhttpd)
