# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(packer ../common/kutil.cc
                      ../common/json.hh

                      asset.hh
                      packer.cc
                      packer.hh
                      output.cc
                      output.hh)
target_link_libraries(packer PUBLIC miniz)
if(NOT EMSCRIPTEN)
    target_link_libraries(packer PUBLIC Threads::Threads)
endif()
if(WIN32)
    target_link_libraries(packer PUBLIC shlwapi)
endif()
