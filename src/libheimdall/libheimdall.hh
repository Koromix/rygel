// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "core.hh"
#include "data.hh"
#include "opengl.hh"
#include "render.hh"
#include "runner.hh"

#ifdef HEIMDALL_IMPLEMENTATION
    #include "../common/kutil.cc"
    #include "core.cc"
    #include "data.cc"
    #include "opengl.cc"
    #include "render.cc"

    #ifdef _WIN32
        #include "runner_win32.cc"
    #endif
#endif
