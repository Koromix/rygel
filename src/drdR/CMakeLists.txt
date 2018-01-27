# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

find_package(R)
if(R_FOUND)
    set(DRDR_SRC ../common/rcpp.cc
                 ../common/rcpp.hh
                 drdR.cc
                 drdR.R)

    R_add_package(drdR DESCRIPTION RCPP ${DRDR_SRC})
    target_link_libraries(drdR PRIVATE drd miniz)
endif()
