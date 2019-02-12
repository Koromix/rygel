# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

find_package(R)
if(NOT R_FOUND)
    return()
endif()

set(DRDR_SRC
    ../wrappers/Rcc.cc
    ../wrappers/Rcc.hh
    drdR_mco.cc
    drdR_mco.R
)

R_add_package(drdR DESCRIPTION NAMESPACE RCPP_INCLUDE ${DRDR_SRC})
target_link_libraries(drdR PRIVATE cc drd miniz)
