# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (EMSCRIPTEN)
    return()
endif()

set(THOP_BASE_URL / CACHE STRING "Base URL used to access THOP")
set(THOP_SHOW_USER ON CACHE BOOL "Enable user support in THOP")

if(THOP_SHOW_USER)
    set(THOP_SHOW_USER 1)
else()
    set(THOP_SHOW_USER 0)
endif()
configure_file(client/thop.html "${CMAKE_CURRENT_BINARY_DIR}/thop.html")
configure_file(client/thop.js "${CMAKE_CURRENT_BINARY_DIR}/thop.js")

set(THOP_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/thop.html"

                   client/util.js
                   "${CMAKE_CURRENT_BINARY_DIR}/thop.js"
                   client/data.js
                   client/user.js
                   client/mco_common.js
                   client/mco_casemix.js
                   client/mco_list.js
                   client/mco_pricing.js
                   client/mco_tree.js
                   client/wt_data_table.js
                   client/wt_pager.js
                   client/wt_period_picker.js
                   client/wt_tree_selector.js
                   client/wt_version_line.js
                   ../../lib/chartjs/chart.min.js

                   client/thop.css
                   client/mco_casemix.css
                   client/mco_list.css
                   client/mco_pricing.css
                   client/mco_tree.css
                   client/wt_data_table.css
                   client/wt_pager.css
                   client/wt_period_picker.css
                   client/wt_tree_selector.css
                   client/wt_version_line.css

                   client/images/thop_logo.svg
                   client/images/favicon.ico)

set(THOP_SRC server/thop.cc
             server/thop.hh
             server/config.cc
             server/config.hh
             server/json.hh
             server/user.cc
             server/user.hh
             server/thop_mco_casemix.cc
             server/thop_mco_tables.cc
             server/thop_mco.hh)

add_custom_command(OUTPUT assets.cc
                   COMMAND $<TARGET_FILE:packer>
                   ARGS -Mthop -mcss,js -e -cgzip -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${THOP_RESOURCES}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   DEPENDS packer ${THOP_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(thop_assets MODULE "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
    set_target_properties(thop_assets PROPERTIES PREFIX "")
else()
    list(APPEND THOP_SRC "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
endif()

add_executable(thop ${THOP_SRC} ${THOP_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
    target_link_libraries(thop PRIVATE dl)
endif()
target_compile_definitions(thop PRIVATE -DTHOP_BASE_URL="${THOP_BASE_URL}")
target_include_directories(thop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(thop PUBLIC drd microhttpd sodium)
