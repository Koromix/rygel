# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (EMSCRIPTEN)
    return()
endif()

set(DRDW_RESOURCES client/drdw.html
                   # Keep config.js first
                   client/config.js
                   client/common.css
                   client/common.js
                   client/user.js
                   client/mco_common.js
                   client/mco_casemix.css
                   client/mco_casemix.js
                   client/mco_list.css
                   client/mco_list.js
                   client/mco_pricing.css
                   client/mco_pricing.js
                   client/mco_tree.css
                   client/mco_tree.js
                   client/wt_period_picker.css
                   client/wt_period_picker.js
                   client/wt_tree_selector.css
                   client/wt_tree_selector.js
                   client/wt_version_line.css
                   client/wt_version_line.js
                   client/images/favicon.ico
                   client/images/logo.png
                   ../../lib/chartjs/chart.min.js)
set(DRDW_SRC server/drdw.cc
             server/drdw.hh
             server/config.cc
             server/config.hh
             server/json.hh
             server/user.cc
             server/user.hh
             server/drdw_mco.cc
             server/drdw_mco.hh)

add_custom_command(OUTPUT assets.cc
                   COMMAND $<TARGET_FILE:packer>
                   ARGS -Mdrdw -mcss,js -e -cgzip -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${DRDW_RESOURCES}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   DEPENDS packer ${DRDW_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(drdw_assets MODULE "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
    set_target_properties(drdw_assets PROPERTIES PREFIX "")
else()
    list(APPEND DRDW_SRC "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
endif()

add_executable(drdw ${DRDW_SRC} ${DRDW_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
    target_link_libraries(drdw PRIVATE dl)
endif()
target_include_directories(drdw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(drdw PUBLIC drd microhttpd sodium)
