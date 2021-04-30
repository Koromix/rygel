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

namespace RG {

extern "C" const char *CAcert_PEM;

static bool sms_ssl = false;
static mbedtls_x509_crt sms_pem;
static mbedtls_x509_crl sms_crl;

bool sms_Config::Validate() const
{
    bool valid = true;

    if (!sid) {
        LogError("Twilio SID is not set");
        valid = false;
    }
    if (!token) {
        LogError("Twilio AuthToken is not set");
        valid = false;
    }
    if (!from) {
        LogError("Twilio From setting is not set");
        valid = false;
    }

    return valid;
}

bool sms_Sender::Init(const sms_Config &config)
{
    // Validate configuration
    if (!config.Validate())
        return false;

    if (!sms_ssl) {
        // Just memset calls it seems, but let's do it as they want
        mbedtls_x509_crt_init(&sms_pem);
        mbedtls_x509_crl_init(&sms_crl);
        atexit([]() {
            mbedtls_x509_crt_free(&sms_pem);
            mbedtls_x509_crl_free(&sms_crl);
        });

        if (mbedtls_x509_crt_parse(&sms_pem, (const uint8_t *)CAcert_PEM, strlen(CAcert_PEM) + 1)) {
            LogError("Failed to parse CA store file");
            return false;
        }

        sms_ssl = true;
    }

    str_alloc.ReleaseAll();
    this->config.sid = DuplicateString(config.sid, &str_alloc).ptr;
    this->config.token = DuplicateString(config.token, &str_alloc).ptr;
    this->config.from = DuplicateString(config.from, &str_alloc).ptr;

    return true;
}

static void EncodeUrlSafe(const char *str, HeapArray<char> *out_buf)
{
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (IsAsciiAlphaOrDigit(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out_buf->Append(c);
        } else {
            Fmt(out_buf, "%%%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

bool sms_Sender::Send(const char *to, const char *message)
{
    RG_ASSERT(config.sid);

    BlockAllocator temp_alloc;

    const char *url;
    const char *body;
    {
        HeapArray<char> buf(&temp_alloc);

        Fmt(&buf, "To=%1&From=%2&Body=", to, config.from);
        EncodeUrlSafe(message, &buf);

        url = Fmt(&temp_alloc, "https://api.twilio.com/2010-04-01/Accounts/%1/Messages", config.sid).ptr;
        body = buf.Leak().ptr;
    }

    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::bad_alloc();
    RG_DEFER { curl_easy_cleanup(curl); };

    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
        success &= !curl_easy_setopt(curl, CURLOPT_POST, 1L);
        success &= !curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        success &= !curl_easy_setopt(curl, CURLOPT_USERNAME, config.sid);
        success &= !curl_easy_setopt(curl, CURLOPT_PASSWORD, config.token);
        success &= !curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        // curl_easy_setopt is variadic, so we need the + lambda operator to force the
        // conversion to a C-style function pointer.
        success &= !curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, +[](CURL *, void *ctx, void *) {
            mbedtls_ssl_config *ssl = (mbedtls_ssl_config *)ctx;
            mbedtls_ssl_conf_ca_chain(ssl, &sms_pem, &sms_crl);
            return CURLE_OK;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *, size_t size, size_t nmemb, void *) { return size * nmemb; });

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
