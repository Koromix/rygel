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

#include "../../core/libcc/libcc.hh"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
#endif
#include "../../../vendor/curl/include/curl/curl.h"
#include "../../../vendor/mbedtls/include/mbedtls/ssl.h"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "sms.hh"
#include "http_misc.hh"

namespace RG {

extern "C" const AssetInfo CacertPem;

bool sms_Config::Validate() const
{
    bool valid = true;

    if (provider == sms_Provider::None) {
        LogError("SMS Provider is not set");
        valid = false;
    }
    if (!authid) {
        LogError("SMS AuthID is not set");
        valid = false;
    }
    if (!token) {
        LogError("SMS AuthToken is not set");
        valid = false;
    }
    if (!from) {
        LogError("SMS From setting is not set");
        valid = false;
    }

    return valid;
}

bool sms_Sender::Init(const sms_Config &config)
{
    // Validate configuration
    if (!config.Validate())
        return false;

    str_alloc.ReleaseAll();
    this->config.provider = config.provider;
    this->config.authid = DuplicateString(config.authid, &str_alloc).ptr;
    this->config.token = DuplicateString(config.token, &str_alloc).ptr;
    this->config.from = DuplicateString(config.from, &str_alloc).ptr;

    return true;
}

bool sms_Sender::Send(const char *to, const char *message)
{
    RG_ASSERT(config.provider != sms_Provider::None);

    switch (config.provider) {
        case sms_Provider::None: { RG_UNREACHABLE(); } break;
        case sms_Provider::Twilio: return SendTwilio(to, message);
    }

    RG_UNREACHABLE();
}

bool sms_Sender::SendTwilio(const char *to, const char *message)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::bad_alloc();
    RG_DEFER { curl_easy_cleanup(curl); };

    const char *url;
    const char *body;
    {
        HeapArray<char> buf(&temp_alloc);

        Fmt(&buf, "To=%1&From=%2&Body=", to, config.from);
        http_EncodeUrlSafe(message, &buf);

        url = Fmt(&temp_alloc, "https://api.twilio.com/2010-04-01/Accounts/%1/Messages", config.authid).ptr;
        body = buf.Leak().ptr;
    }

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
        success &= !curl_easy_setopt(curl, CURLOPT_POST, 1L);
        success &= !curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        success &= !curl_easy_setopt(curl, CURLOPT_USERNAME, config.authid);
        success &= !curl_easy_setopt(curl, CURLOPT_PASSWORD, config.token);
        success &= !curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        // Give embedded CA store to curl
        {
            struct curl_blob blob;

            blob.data = (void *)CacertPem.data.ptr;
            blob.len = CacertPem.data.len;
            blob.flags = CURL_BLOB_NOCOPY;

            success &= !curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
        }

        // curl_easy_setopt is variadic, so we need the + lambda operator to force the
        // conversion to a C-style function pointer.
        success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                     +[](char *, size_t size, size_t nmemb, void *) { return size * nmemb; });

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LogError("Failed to perform SMS call: %1", curl_easy_strerror(res));
        return false;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200 && status != 201) {
        LogError("Failed to send SMS with status %1", status);
        return false;
    }

    LogDebug("Sent SMS to %1", to);
    return true;
}

}
