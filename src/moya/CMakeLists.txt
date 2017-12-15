# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wsign-conversion -Wconversion -Wsign-promo \
                                            -Wno-missing-field-initializers -Wno-old-style-cast")
endif()

set(MOYA_SRC libmoya/a_classifier.cc
             libmoya/a_classifier.hh
             libmoya/a_constraints.cc
             libmoya/a_constraints.hh
             libmoya/d_authorizations.cc
             libmoya/d_authorizations.hh
             libmoya/d_codes.hh
             libmoya/d_desc.cc
             libmoya/d_desc.hh
             libmoya/d_prices.cc
             libmoya/d_prices.hh
             libmoya/d_stays.cc
             libmoya/d_stays.hh
             libmoya/d_tables.cc
             libmoya/d_tables.hh
             ../common/kutil.cc
             ../common/kutil.hh
             libmoya/main.cc
             libmoya/main.hh)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(moya SHARED ${MOYA_SRC})
else()
    add_library(moya STATIC ${MOYA_SRC})
endif()
target_link_libraries(moya PUBLIC miniz)
add_amalgamated_file(moya "${CMAKE_BINARY_DIR}/libmoya.hh" libmoya/libmoya.hh)

set(DRD_SRC drd/drd.cc
            drd/dump.cc
            drd/dump.hh)
add_executable(drd ${DRD_SRC})
target_link_libraries(drd PUBLIC moya)

# Application HTML must be kept first
set(TALYN_RESOURCES talyn/static/talyn.html
                    talyn/static/talyn.css
                    talyn/static/talyn.js
                    resources/logo.png
                    ../../lib/chartjs/chart.min.js)
set(TALYN_SRC talyn/talyn.cc
              talyn/resources.hh)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_resources.cmake" [=[
set(RESOURCES_CC_FILENAME "${BIN_DIR}/talyn/resources.cc")

file(WRITE ${RESOURCES_CC_FILENAME} "\
#include \"${SRC_DIR}/libmoya/libmoya.hh\"\n\
#include \"${SRC_DIR}/talyn/resources.hh\"\n\
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
    OUTPUT talyn/resources.cc
    COMMAND ${CMAKE_COMMAND}
    ARGS -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DRESOURCES="${TALYN_RESOURCES}"
         -P "${CMAKE_CURRENT_BINARY_DIR}/build_talyn_resources.cmake"
    DEPENDS ${TALYN_RESOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    add_library(talyn_res SHARED "${CMAKE_CURRENT_BINARY_DIR}/talyn/resources.cc")
    set_target_properties(talyn_res PROPERTIES PREFIX "")
else()
    list(APPEND TALYN_SRC "${CMAKE_CURRENT_BINARY_DIR}/talyn/resources.cc")
endif()
add_executable(talyn ${TALYN_SRC} ${TALYN_RESOURCES})
target_include_directories(talyn PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/talyn")
target_link_libraries(talyn PUBLIC moya libmicrohttpd)
