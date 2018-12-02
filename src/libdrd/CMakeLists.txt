# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBDRD_SRC ../common/kutil.cc
               ../common/kutil.hh
               common.hh
               mco_authorization.cc
               mco_authorization.hh
               mco_classifier.cc
               mco_classifier.hh
               mco_common.hh
               mco_dump.cc
               mco_dump.hh
               mco_mapper.cc
               mco_mapper.hh
               mco_pricing.cc
               mco_pricing.hh
               mco_stay.cc
               mco_stay.hh
               mco_tables.cc
               mco_tables.hh)
add_library(drd ${LIBDRD_SRC})
set_target_properties(drd PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(drd PUBLIC miniz)
if(NOT EMSCRIPTEN)
    target_link_libraries(drd PUBLIC Threads::Threads)
endif()
add_amalgamated_file(drd "${CMAKE_BINARY_DIR}/libdrd.cc" libdrd.cc)
add_amalgamated_file(drd "${CMAKE_BINARY_DIR}/libdrd.hh" libdrd.hh)
