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

static bool ConfigureSSL(CURL *curl)
{
    // curl_easy_setopt is variadic, so we need the + lambda operator to force the
    // conversion to a C-style function pointer.
    CURLcode ret = curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, +[](CURL *, void *ctx, void *) {
        mbedtls_ssl_config *ssl = (mbedtls_ssl_config *)ctx;
        mbedtls_ssl_conf_ca_chain(ssl, &pem, &crl);
        return CURLE_OK;
    });

    return ret == CURLE_OK;
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

bool SendSMS(const char *sid, const char *token,
             const char *from, const char *to, const char *message)
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
    if (!curl)
        throw std::bad_alloc();
    RG_DEFER { curl_easy_cleanup(curl); };

    {
        bool success = true;

        success &= ConfigureSSL(curl);
        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
        success &= !curl_easy_setopt(curl, CURLOPT_POST, 1L);
        success &= !curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        success &= !curl_easy_setopt(curl, CURLOPT_USERNAME, sid);
        success &= !curl_easy_setopt(curl, CURLOPT_PASSWORD, token);
        success &= !curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
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

static const char *NormalizeAddress(const char *str, Allocator *alloc)
{
    Span<const char> addr = SplitStr(str, ' ');
    return DuplicateString(addr, alloc).ptr;
}

static void EncodeRfc2047(const char *str, HeapArray<char> *out_buf)
{
    out_buf->Append("=?utf-8?Q?");
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (c == ' ') {
            out_buf->Append('_');
        } else if (c >= 32 && c < 128 && c != '=' && c != '?' && c != '_') {
            out_buf->Append(c);
        } else {
            Fmt(out_buf, "=%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }
    out_buf->Append("?=");

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

static void FormatRfcDate(int64_t time, HeapArray<char> *out_buf)
{
    TimeSpec spec = {};
    DecomposeUnixTime(time, TimeMode::Local, &spec);

    switch (spec.week_day) {
        case 1: { out_buf->Append("Mon, "); } break;
        case 2: { out_buf->Append("Tue, "); } break;
        case 3: { out_buf->Append("Wed, "); } break;
        case 4: { out_buf->Append("Thu, "); } break;
        case 5: { out_buf->Append("Fri, "); } break;
        case 6: { out_buf->Append("Sat, "); } break;
        case 7: { out_buf->Append("Sun, "); } break;
    }

    Fmt(out_buf, "%1 ", spec.day);

    switch (spec.month) {
        case 1: { out_buf->Append("Jan "); } break;
        case 2: { out_buf->Append("Feb "); } break;
        case 3: { out_buf->Append("Mar "); } break;
        case 4: { out_buf->Append("Apr "); } break;
        case 5: { out_buf->Append("May "); } break;
        case 6: { out_buf->Append("Jun "); } break;
        case 7: { out_buf->Append("Jul "); } break;
        case 8: { out_buf->Append("Aug "); } break;
        case 9: { out_buf->Append("Sep "); } break;
        case 10: { out_buf->Append("Oct "); } break;
        case 11: { out_buf->Append("Nov "); } break;
        case 12: { out_buf->Append("Dec "); } break;
    }

    int offset = (spec.offset / 60) * 100 + (spec.offset % 60);

    Fmt(out_buf, "%1 %2:%3:%4 %5%6",
                 spec.year, FmtArg(spec.hour).Pad0(-2), FmtArg(spec.min).Pad0(-2),
                 FmtArg(spec.sec).Pad0(-2), offset >= 0 ? "+" : "", FmtArg(offset).Pad0(-4));
}

bool SendMail(const char *url, const char *username, const char *password,
              const char *from, const char *to, const MailContent &content)
{
    BlockAllocator temp_alloc;

    Span<const char> payload;
    {
        HeapArray<char> buf(&temp_alloc);

        char id[33];
        const char *domain;
        {
            uint64_t buf[2];
            randombytes_buf(&buf, RG_SIZE(buf));
            Fmt(id, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));

            SplitStr(from, '@', &domain);
        }

        Fmt(&buf, "Message-ID: <%1@%2>\r\n", id, domain);
        Fmt(&buf, "Date: "); FormatRfcDate(GetUnixTime(), &buf); buf.Append("\r\n");
        Fmt(&buf, "From: "); EncodeRfc2047(from, &buf); buf.Append("\r\n");
        Fmt(&buf, "To: "); EncodeRfc2047(to, &buf); buf.Append("\r\n");
        if (content.subject) {
            Fmt(&buf, "Subject: "); EncodeRfc2047(content.subject, &buf); buf.Append("\r\n");
        }

        if (content.text && content.html) {
            char boundary[17];
            {
                uint64_t buf;
                randombytes_buf(&buf, RG_SIZE(buf));
                Fmt(boundary, "%1", FmtHex(buf).Pad0(-16));
            }

            Fmt(&buf, "Content-Type: multipart/alternative; boundary=\"%1\";\r\n", boundary);
            Fmt(&buf, "MIME-version: 1.0\r\n\r\n");
            Fmt(&buf, "--%1\r\nContent-Type: text/plain; charset=UTF-8;\r\n\r\n", boundary);
            Fmt(&buf, "%1\r\n", content.text);
            Fmt(&buf, "--%1\r\nContent-Type: text/html; charset=UTF-8;\r\n\r\n", boundary);
            Fmt(&buf, "%1\r\n", content.html);
            Fmt(&buf, "--%1--\r\n", boundary);
        } else if (content.html) {
            Fmt(&buf, "Content-Type: text/html; charset=UTF-8;\r\n");
            Fmt(&buf, "MIME-version: 1.0\r\n\r\n");
            Fmt(&buf, "%1\r\n", content.html);
        } else {
            Fmt(&buf, "Content-Type: text/plain; charset=UTF-8;\r\n");
            Fmt(&buf, "MIME-version: 1.0\r\n\r\n");
            Fmt(&buf, "%1\r\n", content.text ? content.text : "");
        }

        payload = buf.Leak();
    }

    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::bad_alloc();
    RG_DEFER { curl_easy_cleanup(curl); };

    // In theory you have to use curl_slist_add, but why do two allocations when none is needed?
    curl_slist recipients = { (char *)NormalizeAddress(to, &temp_alloc), nullptr };

    // Set CURL options
    {
        bool success = true;

        success &= ConfigureSSL(curl);
        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
        if (username) {
            success &= !curl_easy_setopt(curl, CURLOPT_USERNAME, username);
        }
        if (password) {
            success &= !curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
        }
        success &= !curl_easy_setopt(curl, CURLOPT_MAIL_FROM, NormalizeAddress(from, &temp_alloc));
        success &= !curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, &recipients);

        // Give payload to libcurl
        success &= !curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *buf, size_t size, size_t nmemb, void *udata) {
            Span<const char> *payload = (Span<const char> *)udata;

            size_t copy_len = std::min(size * nmemb, (size_t)payload->len);
            memcpy(buf, payload->ptr, copy_len);

            payload->ptr += copy_len;
            payload->len -= (Size)copy_len;

            return copy_len;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_READDATA, payload);
        success &= !curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LogError("Failed to perform mail call: %1", curl_easy_strerror(res));
        return false;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 250) {
        LogError("Failed to send mail with status %1", status);
        return false;
    }

    LogDebug("Sent mail to %1", to);
    return true;
}

}
