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
set(DRDW_SRC drdw.cc)

add_custom_command(OUTPUT assets.cc
                   COMMAND $<TARGET_FILE:packer>
                   ARGS -cgzip -d1 -O"${CMAKE_CURRENT_BINARY_DIR}/assets.cc" ${DRDW_RESOURCES}
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
target_link_libraries(drdw PUBLIC drd microhttpd)
