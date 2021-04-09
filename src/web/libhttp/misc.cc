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
#include "misc.hh"

namespace RG {

const char *http_GetMimeType(Span<const char> extension, const char *default_type)
{
    static const HashMap<Span<const char>, const char *> mime_types = {
        {".txt", "text/plain"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".ico", "image/vnd.microsoft.icon"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".map", "application/json"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".manifest", "application/manifest+json"},
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

static void ReleaseDataCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

void http_JsonPageBuilder::Finish(http_IO *io)
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
