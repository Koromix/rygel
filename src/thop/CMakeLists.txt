# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (EMSCRIPTEN)
    return()
endif()

set(THOP_RESOURCES
    client/thop.html
    ../libweb/compat.js
    ../libweb/util.js
    client/common.js
    client/thop.js
    client/user.js
    client/mco_casemix.js
    client/mco_list.js
    client/mco_pricing.js
    client/mco_tree.js
    ../libweb/data_table.js
    ../libweb/pager.js
    ../libweb/period_picker.js
    ../libweb/tree_selector.js
    ../libweb/version_line.js
    ../../lib/chartjs/chart.min.js
    ../../lib/sheetjs/xlsx.core.min.js
    client/thop.css
    client/mco_casemix.css
    client/mco_list.css
    client/mco_pricing.css
    client/mco_tree.css
    ../libweb/data_table.css
    ../libweb/pager.css
    ../libweb/period_picker.css
    ../libweb/tree_selector.css
    ../libweb/version_line.css
    client/images/thop_logo.svg
    client/images/favicon.png
)
set(THOP_SRC
    ../libcc/json.hh
    server/config.cc
    server/config.hh
    server/mco.cc
    server/mco.hh
    server/mco_casemix.cc
    server/mco_casemix.hh
    server/mco_info.cc
    server/mco_info.hh
    server/response.cc
    server/response.hh
    server/structure.cc
    server/structure.hh
    server/thop.cc
    server/thop.hh
    server/user.cc
    server/user.hh
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(OUTPUT assets.cc
                       COMMAND $<TARGET_FILE:packer>
                       ARGS -Mclient/packer.ini --source_map -e -cgzip -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${THOP_RESOURCES}
                       WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                       DEPENDS packer client/packer.ini ${THOP_RESOURCES})
    add_library(thop_assets MODULE "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
    set_target_properties(thop_assets PROPERTIES PREFIX "")
else()
    add_custom_command(OUTPUT assets.cc
                       COMMAND $<TARGET_FILE:packer>
                       ARGS -Mclient/packer.ini -e -cgzip -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${THOP_RESOURCES}
                       WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                       DEPENDS packer client/packer.ini ${THOP_RESOURCES})
    list(APPEND THOP_SRC "${CMAKE_CURRENT_BINARY_DIR}/assets.cc")
endif()

add_executable(thop ${THOP_SRC} ${THOP_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
    target_link_libraries(thop PRIVATE dl)
endif()
target_include_directories(thop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(thop PUBLIC drd microhttpd sodium)
