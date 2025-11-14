// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "curl.hh"
#include "smtp.hh"
#include "vendor/base64/include/libbase64.h"

namespace K {

static bool CheckURL(const char *url)
{
    CURLU *h = curl_url();
    K_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME);

        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
        if (ret != CURLUE_OK) {
            LogError("Malformed SMTP URL '%1'", url);
            return false;
        }
    }

    // Check scheme
    {
        char *scheme = nullptr;

        CURLUcode ret = curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);
        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
        K_DEFER { curl_free(scheme); };

        if (scheme && !TestStr(scheme, "smtp") && !TestStr(scheme, "smtps")) {
            LogError("Unsupported SMTP scheme '%1'", scheme);
            return false;
        }
    }

    return true;
}

static bool IsAddressSafe(const char *mail)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || IsAsciiControl(c); };

    Span<const char> domain;
    Span<const char> prefix = SplitStr(mail, '@', &domain);

    if (!prefix.len || !domain.len)
        return false;
    if (std::any_of(prefix.begin(), prefix.end(), test_char))
        return false;
    if (std::any_of(domain.begin(), domain.end(), test_char))
        return false;

    return true;
}

static bool IsFileHeaderSafe(Span<const char> str)
{
    const auto test_char = [](char c) { return IsAsciiControl(c); };

    if (!str.len)
        return false;
    if (std::any_of(str.begin(), str.end(), test_char))
        return false;

    return true;
}

bool smtp_Config::Validate() const
{
    bool valid = true;

    if (url) {
        valid = CheckURL(url);
    } else {
        LogError("SMTP URL is not set");
        valid = false;
    }

    if (username && !password) {
        LogError("SMTP username is set without password");
        valid = false;
    }

    if (from) {
        if (!IsAddressSafe(from)) {
            LogError("SMTP From address is invalid");
            valid = false;
        }
    } else {
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

class FmtRfc2047 {
    Span<const char> str;

public:
    FmtRfc2047(Span<const char> str) : str(str) {}

    void Format(FunctionRef<void(Span<const char>)> append) const;
    operator FmtArg() const { return FmtCustom(*this); }
};

class FmtRfcDate {
    int64_t time;

public:
    FmtRfcDate(int64_t time) : time(time) {}

    void Format(FunctionRef<void(Span<const char>)> append) const;
    operator FmtArg() const { return FmtCustom(*this); }
};

void FmtRfc2047::Format(FunctionRef<void(Span<const char>)> append) const
{
    static const char literals[] = "0123456789ABCDEF";

    append("=?utf-8?Q?");
    for (int c: str) {
        if (c == ' ') {
            append('_');
        } else if (strchr("=?_", c) || IsAsciiControl(c)) {
            char encoded[3];

            encoded[0] = '=';
            encoded[1] = literals[((uint8_t)c >> 4) & 0xF];
            encoded[2] = literals[((uint8_t)c >> 0) & 0xF];

            Span<const char> buf = MakeSpan(encoded, 3);
            append(buf);
        } else {
            append((char)c);
        }
    }
    append("?=");
}

void FmtRfcDate::Format(FunctionRef<void(Span<const char>)> append) const
{
    TimeSpec spec = DecomposeTimeLocal(time);

    switch (spec.week_day) {
        case 1: { append("Mon, "); } break;
        case 2: { append("Tue, "); } break;
        case 3: { append("Wed, "); } break;
        case 4: { append("Thu, "); } break;
        case 5: { append("Fri, "); } break;
        case 6: { append("Sat, "); } break;
        case 7: { append("Sun, "); } break;
    }

    Fmt(append, "%1 ", spec.day);

    switch (spec.month) {
        case 1: { append("Jan "); } break;
        case 2: { append("Feb "); } break;
        case 3: { append("Mar "); } break;
        case 4: { append("Apr "); } break;
        case 5: { append("May "); } break;
        case 6: { append("Jun "); } break;
        case 7: { append("Jul "); } break;
        case 8: { append("Aug "); } break;
        case 9: { append("Sep "); } break;
        case 10: { append("Oct "); } break;
        case 11: { append("Nov "); } break;
        case 12: { append("Dec "); } break;
    }

    int offset = (spec.offset / 60) * 100 + (spec.offset % 60);

    Fmt(append, "%1 %2:%3:%4 %5%6",
                spec.year, FmtInt(spec.hour, 2), FmtInt(spec.min, 2),
                FmtInt(spec.sec, 2), offset >= 0 ? "+" : "", FmtInt(offset, 4));
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
    K_ASSERT(config.url);
    K_ASSERT(IsAddressSafe(to));

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    K_DEFER { curl_easy_cleanup(curl); };

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
    K_ASSERT(IsAddressSafe(from));
    K_ASSERT(IsAddressSafe(to));

    HeapArray<char> buf(alloc);

    char id[33];
    const char *domain;
    {
        uint64_t rnd[2];
        FillRandomSafe(&rnd, K_SIZE(rnd));
        Fmt(id, "%1%2", FmtHex(rnd[0], 16), FmtHex(rnd[1], 16));

        SplitStr(from, '@', &domain);
    }

    Fmt(&buf, "Message-ID: <%1@%2>\r\n", id, domain);
    Fmt(&buf, "Date: %1\r\n", FmtRfcDate(GetUnixTime()));
    Fmt(&buf, "From: %1\r\n", from);
    Fmt(&buf, "To: %1\r\n", to);
    if (content.subject) {
        Fmt(&buf, "Subject: %1\r\n", FmtRfc2047(content.subject));
    }
    Fmt(&buf, "MIME-version: 1.0\r\n");

    char mixed[32] = {};
    char alternative[32] = {};

    if (content.files.len) {
        Fmt(mixed, "=_%1", FmtRandom(28));

        Fmt(&buf, "Content-Type: multipart/mixed; boundary=\"%1\";\r\n\r\n", mixed);
        Fmt(&buf, "--%1\r\n", mixed);
    }

    if (content.text && content.html) {
        Fmt(alternative, "=_%1", FmtRandom(28));

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
            K_ASSERT(IsFileHeaderSafe(file.mimetype));
            K_ASSERT(file.id || !file.inlined);
            K_ASSERT(!file.id || IsFileHeaderSafe(file.id));
            K_ASSERT(!file.name || IsFileHeaderSafe(file.name));

            Fmt(&buf, "--%1\r\n", mixed);
            Fmt(&buf, "Content-Type: %1\r\n", file.mimetype);
            Fmt(&buf, "Content-Transfer-Encoding: base64\r\n");
            if (file.id) {
                Fmt(&buf, "Content-ID: %1\r\n", file.id);
            }
            if (file.name) {
                const char *disposition = file.inlined ? "inline" : "attachment";
                Fmt(&buf, "Content-Disposition: %1; filename=\"%2\"\r\n\r\n", disposition, FmtEscape(file.name, '"'));
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
