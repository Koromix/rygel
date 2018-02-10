# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBDRD_SRC ../common/kutil.cc
               ../common/kutil.hh
               a_classifier.cc
               a_classifier.hh
               a_constraints.cc
               a_constraints.hh
               d_authorizations.cc
               d_authorizations.hh
               d_codes.hh
               d_desc.cc
               d_desc.hh
               d_stays.cc
               d_stays.hh
               d_tables.cc
               d_tables.hh
               main.cc
               main.hh)
add_library(drd ${LIBDRD_SRC})
set_target_properties(drd PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads)
target_link_libraries(drd PUBLIC miniz Threads::Threads rpmalloc)
add_amalgamated_file(drd "${CMAKE_BINARY_DIR}/libdrd.hh" libdrd.hh)
enable_unity_build(drd)
