// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
#endif
#include "vendor/curl/include/curl/curl.h"

namespace K {

CURL *curl_Init();
bool curl_Reset(CURL *curl);

int curl_Perform(CURL *curl, const char *reason);

Span<const char> curl_GetUrlPartStr(CURLU *h, CURLUPart part, Allocator *alloc);
int curl_GetUrlPartInt(CURLU *h, CURLUPart part);

}
