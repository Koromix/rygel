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
#include "smtp.hh"
#include "vendor/base64/include/libbase64.h"

namespace RG {

bool smtp_Config::Validate() const
{
    bool valid = true;

    if (!url) {
        LogError("SMTP URL is not set");
        valid = false;
    }
    if (username && !password) {
        LogError("SMTP username is set without password");
        valid = false;
    }
    if (!from) {
        LogError("SMTP From setting is not set");
        valid = false;
    }

    return valid;
}

bool smtp_Sender::Init(const smtp_Config &config)
{
    // Validate configuration
    if (!config.Validate())
        return false;

    str_alloc.Reset();
    this->config.url = DuplicateString(config.url, &str_alloc).ptr;
    this->config.username = DuplicateString(config.username, &str_alloc).ptr;
    this->config.password = DuplicateString(config.password, &str_alloc).ptr;
    this->config.from = DuplicateString(config.from, &str_alloc).ptr;

    return true;
}

static void EncodeRfc2047(const char *str, HeapArray<char> *out_buf)
{
    out_buf->Append("=?utf-8?Q?");
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (c == ' ') {
            out_buf->Append('_');
        } else if (c >= 32 && c < 128 && c != '=' && c != '?' && c != '_') {
            out_buf->Append((char)c);
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
    TimeSpec spec = DecomposeTimeLocal(time);

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

bool smtp_Sender::Send(const char *to, const smtp_MailContent &content)
{
    BlockAllocator temp_alloc;

    // This cannot fail (unless memory runs out)
    Span<const char> mail = smtp_BuildMail(config.from, to, content, &temp_alloc);

    return Send(to, mail);
}

bool smtp_Sender::Send(const char *to, Span<const char> mail)
{
    RG_ASSERT(config.url);

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    // In theory you have to use curl_slist_add, but why do two allocations when none is needed?
    curl_slist recipients = { (char *)to, nullptr };

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, config.url);
        if (config.username) {
            success &= !curl_easy_setopt(curl, CURLOPT_USERNAME, config.username);
            success &= !curl_easy_setopt(curl, CURLOPT_PASSWORD, config.password);
        }
        success &= !curl_easy_setopt(curl, CURLOPT_MAIL_FROM, config.from);
        success &= !curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, &recipients);

        success &= !curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *buf, size_t size, size_t nmemb, void *udata) {
            Span<const char> *payload = (Span<const char> *)udata;

            Size copy_len = std::min((Size)(size * nmemb), payload->len);
            MemCpy(buf, payload->ptr, copy_len);

            payload->ptr += copy_len;
            payload->len -= (Size)copy_len;

            return (size_t)copy_len;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_READDATA, &mail);
        success &= !curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "SMTP");
    if (status < 0)
        return false;
    if (status != 250) {
        LogError("Failed to send mail with status %1", status);
        return false;
    }

    LogDebug("Sent mail to %1", to);
    return true;
}

Span<const char> smtp_BuildMail(const char *from, const char *to, const smtp_MailContent &content, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    char id[33];
    const char *domain;
    {
        uint64_t rnd[2];
        FillRandomSafe(&rnd, RG_SIZE(rnd));
        Fmt(id, "%1%2", FmtHex(rnd[0]).Pad0(-16), FmtHex(rnd[1]).Pad0(-16));

        SplitStr(from, '@', &domain);
    }

    Fmt(&buf, "Message-ID: <%1@%2>\r\n", id, domain);
    Fmt(&buf, "Date: "); FormatRfcDate(GetUnixTime(), &buf); buf.Append("\r\n");
    Fmt(&buf, "From: %1\r\n", from);
    Fmt(&buf, "To: %1\r\n", to);
    if (content.subject) {
        Fmt(&buf, "Subject: "); EncodeRfc2047(content.subject, &buf); buf.Append("\r\n");
    }
    Fmt(&buf, "MIME-version: 1.0\r\n");

    char mixed[32] = {};
    char alternative[32] = {};

    if (content.files.len) {
        uint64_t rnd;
        FillRandomSafe(&rnd, RG_SIZE(rnd));
        Fmt(mixed, "=_%1", FmtHex(rnd).Pad0(-16));

        Fmt(&buf, "Content-Type: multipart/mixed; boundary=\"%1\";\r\n\r\n", mixed);
        Fmt(&buf, "--%1\r\n", mixed);
    }

    if (content.text && content.html) {
        uint64_t rnd;
        FillRandomSafe(&rnd, RG_SIZE(rnd));
        Fmt(alternative, "=_%1", FmtHex(rnd).Pad0(-16));

        Fmt(&buf, "Content-Type: multipart/alternative; boundary=\"%1\";\r\n\r\n", alternative);
        Fmt(&buf, "--%1\r\n", alternative);
        Fmt(&buf, "Content-Type: text/plain; charset=UTF-8;\r\n\r\n");
        Fmt(&buf, "%1\r\n", content.text);
        Fmt(&buf, "--%1\r\n", alternative);
        Fmt(&buf, "Content-Type: text/html; charset=UTF-8;\r\n\r\n");
        Fmt(&buf, "%1\r\n", content.html);
        Fmt(&buf, "--%1--\r\n", alternative);
    } else if (content.html) {
        Fmt(&buf, "Content-Type: text/html; charset=UTF-8;\r\n");
        Fmt(&buf, "%1\r\n", content.html);
    } else {
        Fmt(&buf, "Content-Type: text/plain; charset=UTF-8;\r\n");
        Fmt(&buf, "%1\r\n", content.text ? content.text : "");
    }

    if (content.files.len) {
        for (const smtp_AttachedFile &file: content.files) {
            RG_ASSERT(file.mimetype);
            RG_ASSERT(file.id || !file.inlined);

            Fmt(&buf, "--%1\r\n", mixed);
            Fmt(&buf, "Content-Type: %1\r\n", file.mimetype);
            Fmt(&buf, "Content-Transfer-Encoding: base64\r\n");
            if (file.id) {
                Fmt(&buf, "Content-ID: %1\r\n", file.id);
            }
            if (file.name) {
                const char *disposition = file.inlined ? "inline" : "attachment";
                Fmt(&buf, "Content-Disposition: %1; filename=\"%2\"\r\n\r\n", disposition, file.name);
            } else {
                const char *disposition = file.inlined ? "inline" : "attachment";
                Fmt(&buf, "Content-Disposition: %1\r\n\r\n", disposition);
            }

            base64_state state;
            base64_stream_encode_init(&state, 0);

            for (Size offset = 0; offset < file.data.len; offset += 16384) {
                Size end = std::min(offset + 16384, file.data.len);
                Span<const uint8_t> view = file.data.Take(offset, end - offset);

                // More than needed but more is better than not enough
                buf.Grow(2 * view.len);

                size_t len;
                base64_stream_encode(&state, (const char *)view.ptr, (size_t)view.len, (char *)buf.end(), &len);

                buf.len += (Size)len;
            }

            // Finalize
            {
                buf.Grow(16);

                size_t len;
                base64_stream_encode_final(&state, (char *)buf.end(), &len);

                buf.len += (Size)len;
            }

            Fmt(&buf, "\r\n");
        }

        Fmt(&buf, "--%1--\r\n", mixed);
    }

    Span<const char> mail = buf.TrimAndLeak(1);
    return mail;
}

}
