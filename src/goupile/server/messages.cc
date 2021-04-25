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
#include "../../../vendor/curl/include/curl/curl.h"
#include "../../../vendor/mbedtls/include/mbedtls/ssl.h"
#include "messages.hh"

namespace RG {

static mbedtls_x509_crt pem;
static mbedtls_x509_crl crl;

bool InitSSL()
{
    const AssetInfo *asset = FindPackedAsset("vendor/cacert/cacert.pem");
    RG_ASSERT(asset);

    HeapArray<uint8_t> buf;
    StreamReader reader(asset->data, nullptr, asset->compression_type);
    StreamWriter writer(&buf, nullptr);

    bool success = SpliceStream(&reader, -1, &writer);
    RG_ASSERT(success);
    buf.Append(0); // The PEM parser expects this or it fails weirdly

    // Just memset calls it seems, but let's do it as they want
    mbedtls_x509_crt_init(&pem);
    mbedtls_x509_crl_init(&crl);
    atexit([]() {
        mbedtls_x509_crt_free(&pem);
        mbedtls_x509_crl_free(&crl);
    });

    if (mbedtls_x509_crt_parse(&pem, buf.ptr, (size_t)buf.len)) {
        LogError("Failed to parse CA store file");
        return false;
    }

    return true;
}

static void EncodeUrlSafe(const char *str, HeapArray<char> *out_buf)
{
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (IsAsciiAlphaOrDigit(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out_buf->Append(c);
        } else {
            Fmt(out_buf, "%%%1", FmtHex(c).Pad0(-2));
        }
    }

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

bool SendSMS(const char *sid, const char *token, const char *from,
             const char *to, const char *message)
{
    BlockAllocator temp_alloc;

    const char *url;
    const char *body;
    {
        HeapArray<char> buf(&temp_alloc);

        Fmt(&buf, "To=%1&From=%2&Body=", to, from);
        EncodeUrlSafe(message, &buf);

        url = Fmt(&temp_alloc, "https://api.twilio.com/2010-04-01/Accounts/%1/Messages", sid).ptr;
        body = buf.Leak().ptr;
    }

    CURL *curl = curl_easy_init();
    RG_DEFER { curl_easy_cleanup(curl); };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_USERNAME, sid);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, token);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    // curl_easy_setopt is variadic, so we need the + lambda operator to force the
    // conversion to a C-style function pointer.
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, +[](CURL *, void *ctx, void *) {
        mbedtls_ssl_config *ssl = (mbedtls_ssl_config *)ctx;
        mbedtls_ssl_conf_ca_chain(ssl, &pem, &crl);
        return CURLE_OK;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *, size_t size, size_t nmemb, void *) {
        return size * nmemb;
    });

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
