# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (TARGET heimdall)
    find_package(R)
    if(R_FOUND)
        set(HEIMDALLR_SRC ../common/kutil.cc
                          ../common/rcpp.cc
                          heimdallR.cc
                          heimdallR.R)

        R_add_package(heimdallR DESCRIPTION RCPP ${HEIMDALLR_SRC})
        target_link_libraries(heimdallR PRIVATE heimdall miniz)
    endif()
endif()
