# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(gen_password_hash gen_password_hash.c)
target_link_libraries(gen_password_hash PRIVATE sodium)
