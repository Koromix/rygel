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
#include "http_misc.hh"

namespace RG {

const char *http_GetMimeType(Span<const char> extension, const char *default_type)
{
    static const HashMap<Span<const char>, const char *> mime_types = {
        #define MIMETYPE(Extension, MimeType) { (Extension), (MimeType) },
        #include "mimetypes.inc"

        {"", "application/octet-stream"}
    };

    const char *mime_type = mime_types.FindValue(extension, nullptr);

    if (!mime_type) {
        LogError("Unknown MIME type for extension '%1'", extension);
        mime_type = default_type;
    }

    return mime_type;
}

// Mostly compliant, respects 'q=0' weights but it does not care about ordering beyond that. The
// caller is free to choose a preferred encoding among acceptable ones.
uint32_t http_ParseAcceptableEncodings(Span<const char> encodings)
{
    encodings = TrimStr(encodings);

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
            } else if (encoding == "*") {
                low_priority = ApplyMask(low_priority, UINT32_MAX, quality != "q=0");
            }
        }

        acceptable_encodings = high_priority | low_priority;
    } else {
        acceptable_encodings = UINT32_MAX;
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
        if (RG_UNLIKELY(!out_ranges->Available())) {
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

static void ReleaseDataCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

bool http_JsonPageBuilder::Init(http_IO *io)
{
    RG_ASSERT(!this->io);

    CompressionType encoding;
    if (!io->NegociateEncoding(CompressionType::Gzip, &encoding))
        return false;
    if (!st.Open(&buf, nullptr, encoding))
        return false;

    this->io = io;
    return true;
}

void http_JsonPageBuilder::Finish()
{
    CompressionType compression_type = st.GetCompressionType();

    Flush();

    bool success = st.Close();
    RG_ASSERT(success);

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, buf.ptr,
                                                           ReleaseDataCallback);
    buf.Leak();

    io->AttachResponse(200, response);
    io->AddEncodingHeader(compression_type);
    io->AddHeader("Content-Type", "application/json");
}

}
