// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "curl.hh"

namespace RG {

extern "C" const AssetInfo CacertPem;

CURL *curl_Init()
{
    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::bad_alloc();
    RG_DEFER_N(err_guard) { curl_easy_cleanup(curl); };

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

#if 0
    success &= !curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#else
    // curl_easy_setopt is variadic, so we need the + lambda operator to force the
    // conversion to a C-style function pointers.
    success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                 +[](char *, size_t size, size_t nmemb, void *) { return size * nmemb; });
#endif

    if (!success) {
        LogError("Failed to set libcurl options");
        return nullptr;
    }

    err_guard.Disable();
    return curl;
}

int curl_Perform(CURL *curl, const char *reason, FunctionRef<bool(int, int)> retry)
{
    CURLcode res = curl_easy_perform(curl);

#if 0
    const char *method = nullptr;
    const char *url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_METHOD, &method);
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);

    LogDebug("Curl: %1 %2", method, url);
#endif

    for (int i = 0;; i++) {
        if (res != CURLE_OK) {
            if (res != CURLE_WRITE_ERROR) {
                LogError("Failed to perform %1 call: %2", reason, curl_easy_strerror(res));
            }
            return -1;
        }

        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        if (!retry.IsValid() || !retry(i, (int)status))
            return (int)status;

        int delay = 200 + 200 * (std::min(i, 5) << 3);
        delay += GetRandomIntSafe(0, delay);

        WaitDelay(delay);
    }

    RG_UNREACHABLE();
}

Span<const char> curl_GetUrlPartStr(CURLU *h, CURLUPart part, Allocator *alloc)
{
    char *buf = nullptr;

    CURLUcode ret = curl_url_get(h, part, &buf, 0);
    if (ret == CURLUE_OUT_OF_MEMORY)
        throw std::bad_alloc();
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
        throw std::bad_alloc();
    RG_DEFER { curl_free(buf); };

    int value = -1;
    if (buf) {
        ParseInt(buf, &value);
    }
    return value;
}

}
