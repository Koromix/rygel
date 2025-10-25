// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"

namespace K {

// Mostly compliant, respects 'q=0' weights but it does not care about ordering beyond that. The
// caller is free to choose a preferred encoding among acceptable ones.
uint32_t http_ParseAcceptableEncodings(Span<const char> encodings)
{
    encodings = TrimStr(encodings);

    const uint32_t AllEncodings = (1u << (int)CompressionType::None) |
                                  (1u << (int)CompressionType::Zlib) |
                                  (1u << (int)CompressionType::Gzip) |
                                  (1u << (int)CompressionType::Brotli) |
                                  (1u << (int)CompressionType::Zstd);

    uint32_t acceptable_encodings;
    if (encodings.len) {
        uint32_t low_priority = 1 << (int)CompressionType::None;
        uint32_t high_priority = 0;
        while (encodings.len) {
            Span<const char> quality;
            Span<const char> encoding = TrimStr(SplitStr(encodings, ',', &encodings));
            encoding = TrimStr(SplitStr(encoding, ';', &quality));
            quality = TrimStr(quality);

            if (encoding == "identity") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::None, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::None, quality != "q=0");
            } else if (encoding == "gzip") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Gzip, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Gzip, quality != "q=0");
            } else if (encoding == "deflate") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Zlib, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Zlib, quality != "q=0");
            } else if (encoding == "br") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Brotli, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Brotli, quality != "q=0");
            } else if (encoding == "zstd") {
                high_priority = ApplyMask(high_priority, 1u << (int)CompressionType::Zstd, quality != "q=0");
                low_priority = ApplyMask(low_priority, 1u << (int)CompressionType::Zstd, quality != "q=0");
            } else if (encoding == "*") {
                low_priority = ApplyMask(low_priority, AllEncodings, quality != "q=0");
            }
        }

        acceptable_encodings = high_priority | low_priority;
    } else {
        acceptable_encodings = 1u << (int)CompressionType::None;
    }

    return acceptable_encodings;
}

bool http_ParseRange(Span<const char> str, Size len, LocalArray<http_ByteRange, 16> *out_ranges)
{
    K_DEFER_NC(out_guard, len = out_ranges->len) { out_ranges->RemoveFrom(len); };

    Span<const char> unit = TrimStr(SplitStr(str, '=', &str));
    if (unit != "bytes") {
        LogError("HTTP range unit '%1' is not supported", unit);
        return false;
    }

    do {
        if (!out_ranges->Available()) [[unlikely]] {
            LogError("Excessive number of range fragments");
            return false;
        }

        Span<const char> part = TrimStr(SplitStr(str, ',', &str));
        if (!part.len) {
            LogError("Empty HTTP range fragment");
            return false;
        }

        Span<const char> end;
        Span<const char> start = TrimStr(SplitStr(part, '-', &end));
        end = TrimStr(end);

        http_ByteRange range = {};

        if (start.len) {
            if (!ParseInt(start, &range.start))
                return false;
            if (range.start < 0 || range.start > len) {
                LogError("Invalid HTTP range");
                return false;
            }

            if (end.len) {
                if (!ParseInt(end, &range.end))
                    return false;
                if (range.end < 0 || range.end >= len) {
                    LogError("Invalid HTTP range");
                    return false;
                }
                if (range.end < range.start) {
                    LogError("Invalid HTTP range");
                    return false;
                }
                range.end++;
            } else {
                range.end = len;
            }
        } else {
            if (!ParseInt(end, &range.end))
                return false;
            if (range.end < 0 || range.end > len) {
                LogError("Invalid HTTP range");
                return false;
            }

            range.start = len - range.end;
            range.end = len;
        }

        out_ranges->Append(range);
    } while (str.len);

    if (out_ranges->len >= 2) {
        std::sort(out_ranges->begin(), out_ranges->end(),
                  [](const http_ByteRange &range1, const http_ByteRange &range2) {
            return range1.start < range2.start;
        });

        Size j = 1;
        for (Size i = 1; i < out_ranges->len; i++) {
            http_ByteRange &prev = (*out_ranges)[j - 1];
            http_ByteRange &range = (*out_ranges)[i];

            if (range.start < prev.end) {
                LogError("Refusing to serve overlapping ranges");
                return false;
            } else if (range.start == prev.end) {
                prev.end = range.end;
            } else {
                (*out_ranges)[j++] = range;
            }
        }
        out_ranges->RemoveFrom(j);
    }

    out_guard.Disable();
    return true;
}

bool http_PreventCSRF(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    // Try Sec-Fetch-Site header
    {
        const char *sec = request.GetHeaderValue("Sec-Fetch-Site");

        if (sec) {
            if (!TestStr(sec, "same-origin") && !TestStr(sec, "none")) {
                LogError("Denying cross-origin request (Sec-Fetch-Site)");
                io->SendError(403);
                return false;
            }

            return true;
        }
    }

    // Try Origin header
    {
        const char *host = request.GetHeaderValue("Host");
        const char *origin = request.GetHeaderValue("Origin");

        if (host && origin) {
            if (StartsWith(origin, "https://")) {
                origin += 8;
            } else if (StartsWith(origin, "http://")) {
                origin += 7;
            }

            if (host && !TestStr(origin, host)) {
                LogError("Denying cross-origin request (Origin)");
                io->SendError(403);
                return false;
            }

            return true;
        }
    }

    // Assume direct use of web API through curl or something else
    return true;
}

bool http_ParseJson(http_IO *io, int64_t max_len, FunctionRef<bool(json_Parser *json)> func)
{
    StreamReader st;
    if (!io->OpenForRead(max_len, &st))
        return false;
    json_Parser json(&st, io->Allocator());

    if (func(&json)) {
        K_ASSERT(json.IsValid());
        return true;
    } else {
        return false;
    }
}

bool http_SendJson(http_IO *io, int status, FunctionRef<void(json_Writer *json)> func)
{
    CompressionType encoding;
    if (!io->NegociateEncoding(CompressionType::Brotli, CompressionType::Gzip, &encoding))
        return false;
    io->AddHeader("Content-Type", "application/json");

    StreamWriter st;
    if (!io->OpenForWrite(status, encoding, -1, &st))
        return false;

    json_Writer json(&st);
    func(&json);

    return st.Close();
}

}
