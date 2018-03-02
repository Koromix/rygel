# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT TARGET heimdall)
    return()
endif()

find_package(R)
if(R_FOUND)
    set(HEIMDALLR_SRC ../common/rcpp.cc
                      ../common/rcpp.hh
                      heimdallR.cc
                      heimdallR.R)

    R_add_package(heimdallR DESCRIPTION NAMESPACE RCPP ${HEIMDALLR_SRC})
    target_link_libraries(heimdallR PRIVATE heimdall miniz)
endif()
