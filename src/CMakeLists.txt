# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(CORE_SRC core/algorithm.cc
             core/algorithm.hh
             core/codes.hh
             core/kutil.cc
             core/kutil.hh
             core/pricing.cc
             core/pricing.hh
             core/stays.cc
             core/stays.hh
             core/tables.cc
             core/tables.hh)
add_library(moya STATIC ${CORE_SRC})
add_amalgamated_file(moya "${CMAKE_BINARY_DIR}/libmoya.hh" core/libmoya.hh)

set(CLI_SRC cli/dump.cc
            cli/dump.hh
            cli/moya_cli.cc)
add_executable(moya_cli ${CLI_SRC})
set_target_properties(moya_cli PROPERTIES OUTPUT_NAME moya)
target_link_libraries(moya_cli PUBLIC moya)
