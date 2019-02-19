# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The platform layer is only available for Win32, for now

set(GOUPIL_SRC
    ../wrappers/http.cc
    ../wrappers/http.hh
    server/config.cc
    server/config.hh
    server/goupil.cc
)

add_executable(goupil ${GOUPIL_SRC})
target_link_libraries(goupil PRIVATE libcc microhttpd sqlite)

set(GOUPIL_ADMIN_SRC
    admin/goupil_admin.cc
    admin/profile.cc
    admin/profile.hh
    server/config.cc
    server/config.hh
    server/data_sqlite.cc
    server/data.hh
)

add_executable(goupil_admin ${GOUPIL_ADMIN_SRC})
target_link_libraries(goupil_admin PRIVATE libcc sqlite)
