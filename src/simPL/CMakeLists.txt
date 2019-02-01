# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(simPL
    simulator/bone.cc
    simulator/bone.hh
    simulator/cardiovascular.cc
    simulator/cardiovascular.hh
    simulator/death.cc
    simulator/death.hh
    simulator/exam.cc
    simulator/exam.hh
    simulator/flags.hh
    simulator/human.cc
    simulator/human.hh
    simulator/random.cc
    simulator/random.hh
    simulator/simpl.cc
    simulator/simpl.hh
    simulator/utility.cc
    simulator/utility.hh
)
target_link_libraries(simPL PRIVATE libcc)
