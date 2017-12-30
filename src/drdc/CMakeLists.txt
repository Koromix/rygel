# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(DRDC_SRC drdc.cc
             dump.cc
             dump.hh)

add_executable(drdc ${DRDC_SRC})
target_link_libraries(drdc PUBLIC libdrd)
