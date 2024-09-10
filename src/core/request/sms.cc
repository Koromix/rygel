// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "sms.hh"

namespace RG {

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

    str_alloc.Reset();
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

static void EncodeUrlSafe(Span<const char> str, const char *passthrough, HeapArray<char> *out_buf)
{
    for (char c: str) {
        if (IsAsciiAlphaOrDigit(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out_buf->Append((char)c);
        } else if (passthrough && strchr(passthrough, c)) {
            out_buf->Append((char)c);
        } else {
            Fmt(out_buf, "%%%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

bool sms_Sender::SendTwilio(const char *to, const char *message)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    const char *url;
    const char *body;
    {
        HeapArray<char> buf(&temp_alloc);

        Fmt(&buf, "To=%1&From=%2&Body=", to, config.from);
        EncodeUrlSafe(message, nullptr, &buf);

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

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "SMS");
    if (status < 0)
        return false;
    if (status != 200 && status != 201) {
        LogError("Failed to send SMS with status %1", status);
        return false;
    }

    LogDebug("Sent SMS to %1", to);
    return true;
}

}
