// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(_WIN32)
    #include "window_win32.cc"
#elif defined(__EMSCRIPTEN__)
    #include "window_emsdk.cc"
#endif
