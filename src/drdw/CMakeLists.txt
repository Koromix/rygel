# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (EMSCRIPTEN)
    return()
endif()

set(DRDW_RESOURCES drdw.html
                   common.css
                   common.js
                   casemix.css
                   casemix.js
                   list.css
                   list.js
                   pricing.css
                   pricing.js
                   tree.css
                   tree.js
                   user.js
                   w_multiselector.css
                   w_multiselector.js
                   images/favicon.ico
                   images/logo.png
                   ../../lib/chartjs/chart.min.js)
set(DRDW_SRC drdw.cc
             drdw.hh
             casemix.cc
             config.cc
             config.hh
             list.cc
             tree.cc
             user.cc)

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
