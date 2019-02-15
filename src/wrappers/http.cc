// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "http.hh"

static void ReleaseCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

void http_Response::AddEncodingHeader(CompressionType compression_type)
{
    switch (compression_type) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: {
            MHD_add_response_header(response.get(), "Content-Encoding", "deflate");
        } break;
        case CompressionType::Gzip: {
            MHD_add_response_header(response.get(), "Content-Encoding", "gzip");
        } break;
    }
}

void http_Response::AddCookieHeader(const char *path, const char *name, const char *value,
                                    bool http_only)
{
    char cookie_buf[512];
    if (value) {
        Fmt(cookie_buf, "%1=%2; Path=%3; SameSite=Lax;%4",
            name, value, path, http_only ? " HttpOnly;" : "");
    } else {
        Fmt(cookie_buf, "%1=; Path=%2; Max-Age=0;", name, path);
    }

    MHD_add_response_header(response.get(), "Set-Cookie", cookie_buf);
}

const char *http_GetMimeType(Span<const char> extension)
{
    if (extension == ".css") {
        return "text/css";
    } else if (extension == ".html") {
        return "text/html";
    } else if (extension == ".ico") {
        return "image/vnd.microsoft.icon";
    } else if (extension == ".js") {
        return "application/javascript";
    } else if (extension == ".json") {
        return "application/json";
    } else if (extension == ".png") {
        return "image/png";
    } else if (extension == ".svg") {
        return "image/svg+xml";
    } else if (extension == ".map") {
        return "application/json";
    } else {
        LogError("Unknown MIME type for extension '%1'", extension);
        return "application/octet-stream";
    }
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
                low_priority = ApplyMask(high_priority, 1u << (int)CompressionType::None, quality != "q=0");
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

int http_ProduceErrorPage(int code, http_Response *out_response)
{
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1: %2", code,
                          MHD_get_reason_phrase_for((unsigned int)code));

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)page.len, page.ptr,
                                                           ReleaseCallback);
    out_response->response.reset(response);

    MHD_add_response_header(response, "Content-Type", "text/plain");

    return code;
}

int http_ProduceStaticAsset(Span<const uint8_t> data, CompressionType in_compression_type,
                            const char *mime_type, CompressionType out_compression_type,
                            http_Response *out_response)
{
    MHD_Response *response;
    if (in_compression_type != out_compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(data, nullptr, in_compression_type);
            StreamWriter writer(&buf, nullptr, out_compression_type);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return http_ProduceErrorPage(500, out_response);
            if (!writer.Close())
                return http_ProduceErrorPage(500, out_response);
        }

        response = MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, (void *)buf.ptr,
                                                                      ReleaseCallback);
        buf.Leak();
    } else {
        response = MHD_create_response_from_buffer((size_t)data.len, (void *)data.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
    }
    out_response->response.reset(response);

    out_response->AddEncodingHeader(out_compression_type);
    if (mime_type) {
        MHD_add_response_header(*out_response, "Content-Type", mime_type);
    }

    return 200;
}

int http_JsonPageBuilder::Finish(http_Response *out_response)
{
    CompressionType compression_type = st.compression.type;

    Flush();
    Assert(st.Close());

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, buf.ptr,
                                                           ReleaseCallback);
    buf.Leak();
    out_response->response.reset(response);

    out_response->AddEncodingHeader(compression_type);
    MHD_add_response_header(*out_response, "Content-Type", "application/json");

    return 200;
}
