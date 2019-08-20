// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "http.hh"

namespace RG {

static void ReleaseDataCallback(void *ptr)
{
    Allocator::Release(nullptr, ptr, -1);
}

static bool NegociateContentEncoding(MHD_Connection *conn, CompressionType *out_compression_type,
                                     http_IO *io)
{
    const char *accept_str = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)CompressionType::Gzip)) {
        *out_compression_type = CompressionType::Gzip;
        return true;
    } else if (acceptable_encodings) {
        *out_compression_type = (CompressionType)CountTrailingZeros(acceptable_encodings);
        return true;
    } else {
        http_ProduceErrorPage(406, io);
        return false;
    }
}

int http_Daemon::HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                               const char *, const char *upload_data, size_t *upload_data_size,
                               void **con_cls)
{
    http_Daemon *daemon = (http_Daemon *)cls;
    http_RequestInfo *request = *(http_RequestInfo **)con_cls;

    http_IO io = {};

    // Avoid stale messages and messages from other theads in error pages
    ClearLastLogError();

    // Init request data
    if (!request) {
        request = new http_RequestInfo();
        *con_cls = request;

        request->conn = conn;
        request->method = method;

        // Trim URL prefix (base_url setting)
        for (Size i = 0; daemon->base_url[i]; i++, url++) {
            if (url[0] != daemon->base_url[i]) {
                if (!url[0] && daemon->base_url[i] == '/' && !daemon->base_url[i + 1]) {
                    io.AddHeader("Location", daemon->base_url);
                    return MHD_queue_response(conn, 303, io.response.get());
                } else {
                    http_ProduceErrorPage(404, &io);
                    return MHD_queue_response(conn, (unsigned int)io.code, io.response.get());
                }
            }
        }
        request->url = --url;

        if (!NegociateContentEncoding(conn, &request->compression_type, &io))
            return MHD_queue_response(conn, (unsigned int)io.code, io.response.get());
    }

    // Process POST data if any
    if (TestStr(method, "POST")) {
        if (!request->pp) {
            request->pp = MHD_create_post_processor(conn, Kibibytes(32),
                                                    [](void *cls, enum MHD_ValueKind, const char *key,
                                                       const char *, const char *, const char *,
                                                       const char *data, uint64_t, size_t) {
                http_RequestInfo *request = (http_RequestInfo *)cls;

                key = DuplicateString(key, &request->alloc).ptr;
                data = DuplicateString(data, &request->alloc).ptr;
                request->post.Append(key, data);

                return MHD_YES;
            }, request);
            if (!request->pp) {
                http_ProduceErrorPage(422, &io);
                return MHD_queue_response(conn, (unsigned int)io.code, io.response.get());
            }

            return MHD_YES;
        } else if (*upload_data_size) {
            if (MHD_post_process(request->pp, upload_data, *upload_data_size) != MHD_YES) {
                http_ProduceErrorPage(422, &io);
                return MHD_queue_response(conn, (unsigned int)io.code, io.response.get());
            }

            *upload_data_size = 0;
            return MHD_YES;
        }
    }

    // Run real handler
    daemon->handle_func(*request, &io);
    return MHD_queue_response(conn, (unsigned int)io.code, io.response.get());
}

void http_Daemon::RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                   MHD_RequestTerminationCode toe)
{
    const http_Daemon &daemon = *(const http_Daemon *)cls;
    http_RequestInfo *request = (http_RequestInfo *)*con_cls;

    if (request) {
        if (daemon.release_func) {
            daemon.release_func(*request, toe);
        }

        if (request->pp) {
            MHD_destroy_post_processor(request->pp);
        }
        delete request;
    }
}

bool http_Daemon::Start(IPStack stack, int port, int threads, const char *base_url)
{
    RG_ASSERT(!daemon);

    RG_ASSERT_DEBUG(handle_func);
    RG_ASSERT_DEBUG(base_url);

    // Validate configuration
    {
        bool valid = true;

        if (port < 1 || port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", port, UINT16_MAX);
            valid = false;
        }
        if (threads < 0 || threads > 128) {
            LogError("HTTP threads %1 is invalid (range: 0 - 128)", threads);
            valid = false;
        }
        if (base_url[0] != '/' || base_url[strlen(base_url) - 1] != '/') {
            LogError("Base URL '%1' does not start and end with '/'", base_url);
            valid = false;
        }

        if (!valid)
            return false;
    }

    // MHD options
    int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_ALLOW_SUSPEND_RESUME | MHD_USE_ERROR_LOG;
    LocalArray<MHD_OptionItem, 16> mhd_options;
    switch (stack) {
        case IPStack::Dual: { flags |= MHD_USE_DUAL_STACK; } break;
        case IPStack::IPv4: {} break;
        case IPStack::IPv6: { flags |= MHD_USE_IPv6; } break;
    }
    if (!threads) {
        flags |= MHD_USE_THREAD_PER_CONNECTION;
    } else if (threads > 1) {
        mhd_options.Append({MHD_OPTION_THREAD_POOL_SIZE, threads});
    }
    mhd_options.Append({MHD_OPTION_END, 0, nullptr});
#ifndef NDEBUG
    flags |= MHD_USE_DEBUG;
#endif

    daemon = MHD_start_daemon(flags, (int16_t)port, nullptr, nullptr,
                              &http_Daemon::HandleRequest, this,
                              MHD_OPTION_NOTIFY_COMPLETED, &http_Daemon::RequestCompleted, this,
                              MHD_OPTION_ARRAY, mhd_options.data, MHD_OPTION_END);
    this->base_url = base_url;

    return daemon;
}

void http_Daemon::Stop()
{
    if (daemon) {
        MHD_stop_daemon(daemon);
    }
    daemon = nullptr;
}

http_IO::http_IO()
{
    MHD_Response *response0 = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    response.reset(response0);
}

void http_IO::AddHeader(const char *key, const char *value)
{
    MHD_add_response_header(response.get(), key, value);
}

void http_IO::AddEncodingHeader(CompressionType compression_type)
{
    switch (compression_type) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: {
            AddHeader("Content-Encoding", "deflate");
        } break;
        case CompressionType::Gzip: {
            AddHeader("Content-Encoding", "gzip");
        } break;
    }
}

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value,
                              bool http_only)
{
    char cookie_buf[512];
    if (value) {
        Fmt(cookie_buf, "%1=%2; Path=%3; SameSite=Lax;%4",
            name, value, path, http_only ? " HttpOnly;" : "");
    } else {
        Fmt(cookie_buf, "%1=; Path=%2; Max-Age=0;", name, path);
    }

    AddHeader("Set-Cookie", cookie_buf);
}

void http_IO::AddCachingHeaders(int max_age, const char *etag)
{
    RG_ASSERT_DEBUG(max_age >= 0);

    if (!(flags & (int)Flag::EnableCacheControl)) {
        max_age = 0;
    }
    if (!(flags & (int)Flag::EnableETag)) {
        etag = nullptr;
    }

    if (max_age || etag) {
        char buf[512];
        Fmt(buf, "max-age=%1", max_age);
        AddHeader("Cache-Control", buf);

        if (etag) {
            AddHeader("ETag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

void http_IO::AttachResponse(int new_code, MHD_Response *new_response)
{
    code = new_code;

    MHD_move_response_headers(response.get(), new_response);
    response.reset(new_response);
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
    } else if (extension == ".woff") {
        return "font/woff";
    } else if (extension == ".woff2") {
        return "font/woff2";
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

void http_ProduceErrorPage(int code, http_IO *io)
{
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1: %2\n%3", code,
                          MHD_get_reason_phrase_for((unsigned int)code), GetLastLogError());

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)page.len, page.ptr,
                                                           ReleaseDataCallback);
    io->AttachResponse(code, response);
    io->AddHeader("Content-Type", "text/plain");
}

void http_ProduceStaticAsset(Span<const uint8_t> data, CompressionType in_compression_type,
                             const char *mime_type, CompressionType out_compression_type,
                             http_IO *io)
{
    MHD_Response *response;
    if (in_compression_type != out_compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(data, nullptr, in_compression_type);
            StreamWriter writer(&buf, nullptr, out_compression_type);
            if (!SpliceStream(&reader, Megabytes(8), &writer)) {
                http_ProduceErrorPage(500, io);
                return;
            }
            if (!writer.Close()) {
                http_ProduceErrorPage(500, io);
                return;
            }
        }

        response = MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, (void *)buf.ptr,
                                                                      ReleaseDataCallback);
        buf.Leak();
    } else {
        response = MHD_create_response_from_buffer((size_t)data.len, (void *)data.ptr,
                                                   MHD_RESPMEM_PERSISTENT);
    }
    io->AttachResponse(200, response);

    io->AddEncodingHeader(out_compression_type);
    if (mime_type) {
        io->AddHeader("Content-Type", mime_type);
    }

    io->flags |= (int)http_IO::Flag::EnableCache;
}

void http_JsonPageBuilder::Finish(http_IO *io)
{
    CompressionType compression_type = st.compression.type;

    Flush();
    RG_ASSERT(st.Close());

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, buf.ptr,
                                                           ReleaseDataCallback);
    buf.Leak();

    io->AttachResponse(200, response);
    io->AddEncodingHeader(compression_type);
    io->AddHeader("Content-Type", "application/json");
}

}
