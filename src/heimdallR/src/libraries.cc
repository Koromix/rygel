// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WINVER 0x0602
    #define _WIN32_WINNT 0x0602
#endif

#include "../../common/rcpp.cc"

#include "../../../lib/imgui/imgui.cpp"
#include "../../../lib/imgui/imgui_draw.cpp"
#include "../../../lib/imgui/imgui_demo.cpp"

#define HEIMDALL_IMPLEMENTATION
#define KUTIL_NO_MINIZ
#include "../../libheimdall/libheimdall.hh"
