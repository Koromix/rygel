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
#include "sms.hh"

namespace K {

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
    K_ASSERT(config.provider != sms_Provider::None);

    switch (config.provider) {
        case sms_Provider::None: { K_UNREACHABLE(); } break;
        case sms_Provider::Twilio: return SendTwilio(to, message);
    }

    K_UNREACHABLE();
}

bool sms_Sender::SendTwilio(const char *to, const char *message)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    K_DEFER { curl_easy_cleanup(curl); };

    const char *url = Fmt(&temp_alloc, "https://api.twilio.com/2010-04-01/Accounts/%1/Messages", config.authid).ptr;
    const char *body = Fmt(&temp_alloc, "To=%1&From=%2&Body=%3", FmtUrlSafe(to, "-._~"), config.from, FmtUrlSafe(message, "-._~")).ptr;

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
