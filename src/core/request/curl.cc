// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "curl.hh"

#if !defined(_WIN32)
    #include <fcntl.h>
#endif

namespace RG {

extern "C" const AssetInfo CacertPem;

CURL *curl_Init()
{
    CURL *curl = curl_easy_init();
    if (!curl)
        RG_BAD_ALLOC();
    RG_DEFER_N(err_guard) { curl_easy_cleanup(curl); };

    if (!curl_Reset(curl))
        return nullptr;

    err_guard.Disable();
    return curl;
}

bool curl_Reset(CURL *curl)
{
    curl_easy_reset(curl);

    bool success = true;

    // Give embedded CA store to curl
    {
        struct curl_blob blob;

        blob.data = (void *)CacertPem.data.ptr;
        blob.len = CacertPem.data.len;
        blob.flags = CURL_BLOB_NOCOPY;

        success &= !curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
    }

    success &= !curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    success &= !curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    success &= !curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    success &= !curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 60000L);

#if 0
    success &= !curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#else
    // curl_easy_setopt is variadic, so we need the + lambda operator to force the
    // conversion to a C-style function pointers.
    success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                 +[](char *, size_t size, size_t nmemb, void *) { return size * nmemb; });
#endif

#if !defined(_WIN32)
    success &= !curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, +[](void *, curl_socket_t fd, curlsocktype) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        return (int)CURL_SOCKOPT_OK;
    });
#endif

    if (!success) {
        LogError("Failed to set libcurl options");
        return false;
    }

    return true;
}

int curl_Perform(CURL *curl, const char *reason)
{
    CURLcode res = curl_easy_perform(curl);

#if 0
    const char *method = nullptr;
    const char *url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_METHOD, &method);
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);

    LogDebug("Curl: %1 %2", method, url);
#endif

    if (res != CURLE_OK) {
        if (reason && res != CURLE_WRITE_ERROR) {
            LogError("Failed to perform %1 call: %2", reason, curl_easy_strerror(res));
        }
        return -res;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    return (int)status;
}

Span<const char> curl_GetUrlPartStr(CURLU *h, CURLUPart part, Allocator *alloc)
{
    char *buf = nullptr;

    CURLUcode ret = curl_url_get(h, part, &buf, 0);
    if (ret == CURLUE_OUT_OF_MEMORY)
        RG_BAD_ALLOC();
    RG_DEFER { curl_free(buf); };

    if (buf && buf[0]) {
        Span<const char> str = DuplicateString(buf, alloc);
        return str;
    } else {
        return {};
    }
}

int curl_GetUrlPartInt(CURLU *h, CURLUPart part)
{
    char *buf = nullptr;

    CURLUcode ret = curl_url_get(h, part, &buf, 0);
    if (ret == CURLUE_OUT_OF_MEMORY)
        RG_BAD_ALLOC();
    RG_DEFER { curl_free(buf); };

    int value = -1;
    if (buf) {
        ParseInt(buf, &value);
    }
    return value;
}

}
