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
#include "server.hh"
#include "misc.hh"

namespace RG {

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
    RG_DEFER_NC(out_guard, len = out_ranges->len) { out_ranges->RemoveFrom(len); };

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

    const char *xh = request.GetHeaderValue("X-Requested-With");
    const char *sec = request.GetHeaderValue("Sec-Fetch-Site");

    if (!xh || !xh[0]) {
        xh = request.GetHeaderValue("X-Api-Key");

        if (!xh || !xh[0]) {
            LogError("Anti-CSRF header is missing");
            io->SendError(403);
            return false;
        }
    }

    if (sec && !TestStr(sec, "same-origin")) {
        LogError("Denying cross-origin request");
        io->SendError(403);
        return false;
    }

    return true;
}

bool http_JsonPageBuilder::Init(http_IO *io)
{
    RG_ASSERT(!this->io);

    if (!io->NegociateEncoding(CompressionType::Brotli, CompressionType::Gzip, &encoding))
        return false;
    if (!st.Open(&buf, "<json>", 0, encoding))
        return false;

    this->io = io;
    return true;
}

void http_JsonPageBuilder::Finish()
{
    Flush();

    bool success = st.Close();
    RG_ASSERT(success);

    Span<const uint8_t> data = buf.Leak();
    allocator.GiveTo(io->Allocator());

    io->AddEncodingHeader(encoding);
    io->SendBinary(200, data, "application/json");
}

}
