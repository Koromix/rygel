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

static int NegociateContentEncoding(MHD_Connection *conn, CompressionType *out_compression_type,
                                    http_Response *out_response)
{
    const char *accept_str = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)CompressionType::Gzip)) {
        *out_compression_type = CompressionType::Gzip;
        return 0;
    } else if (acceptable_encodings) {
        *out_compression_type = (CompressionType)CountTrailingZeros(acceptable_encodings);
        return 0;
    } else {
        return http_ProduceErrorPage(406, out_response);
    }
}

int http_Daemon::HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                               const char *, const char *upload_data, size_t *upload_data_size,
                               void **con_cls)
{
    http_Daemon *daemon = (http_Daemon *)cls;
    http_Request *request = *(http_Request **)con_cls;

    http_Response response = {};

    // Init request data
    if (!request) {
        request = new http_Request();
        *con_cls = request;

        request->conn = conn;
        request->method = method;

        // Trim URL prefix (base_url setting)
        for (Size i = 0; daemon->base_url[i]; i++, url++) {
            if (url[0] != daemon->base_url[i]) {
                if (!url[0] && daemon->base_url[i] == '/' && !daemon->base_url[i + 1]) {
                    MHD_Response *response =
                        MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
                    MHD_add_response_header(response, "Location", daemon->base_url);
                    return 303;
                } else {
                    return http_ProduceErrorPage(404, &response);
                }
            }
        }
        request->url = --url;

        if (int code = NegociateContentEncoding(conn, &request->compression_type, &response); code)
            return MHD_queue_response(conn, (unsigned int)code, response.response.get());
    }

    // Process POST data if any
    if (TestStr(method, "POST")) {
        if (!request->pp) {
            request->pp = MHD_create_post_processor(conn, Kibibytes(32),
                                                    [](void *cls, enum MHD_ValueKind, const char *key,
                                                       const char *, const char *, const char *,
                                                       const char *data, uint64_t, size_t) {
                http_Request *request = (http_Request *)cls;

                key = DuplicateString(key, &request->alloc).ptr;
                data = DuplicateString(data, &request->alloc).ptr;
                request->post.Append(key, data);

                return MHD_YES;
            }, request);
            if (!request->pp) {
                http_ProduceErrorPage(422, &response);
                return MHD_queue_response(conn, 422, response.response.get());
            }

            return MHD_YES;
        } else if (*upload_data_size) {
            if (MHD_post_process(request->pp, upload_data, *upload_data_size) != MHD_YES) {
                http_ProduceErrorPage(422, &response);
                return MHD_queue_response(conn, 422, response.response.get());
            }

            *upload_data_size = 0;
            return MHD_YES;
        }
    }

    // Only cache GET requests by default
    if (!TestStr(request->method, "GET")) {
        response.flags |= (int)http_Response::Flag::DisableCache;
    }

    // Run real handler
    int code = daemon->handle_func(*request, &response);
    return MHD_queue_response(conn, (unsigned int)code, response.response.get());
}

void http_Daemon::RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                   MHD_RequestTerminationCode toe)
{
    const http_Daemon &daemon = *(const http_Daemon *)cls;
    http_Request *request = (http_Request *)*con_cls;

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

    RG_DEBUG_ASSERT(handle_func);
    RG_DEBUG_ASSERT(base_url);

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
    int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_USE_ERROR_LOG;
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

void http_Response::AddCachingHeaders(int max_age, const char *etag)
{
    RG_DEBUG_ASSERT(max_age >= 0);

    if (flags & (int)http_Response::Flag::DisableCacheControl) {
        max_age = 0;
    }
    if (flags & (int)http_Response::Flag::DisableETag) {
        etag = nullptr;
    }

    if (max_age || etag) {
        char buf[512];
        Fmt(buf, "max-age=%1", max_age);
        MHD_add_response_header(response.get(), "Cache-Control", buf);

        if (etag) {
            MHD_add_response_header(response.get(), "ETag", etag);
        }
    } else {
        MHD_add_response_header(response.get(), "Cache-Control", "no-store");
    }
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
                                                           ReleaseDataCallback);
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
                                                                      ReleaseDataCallback);
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
    RG_ASSERT(st.Close());

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)buf.len, buf.ptr,
                                                           ReleaseDataCallback);
    buf.Leak();
    out_response->response.reset(response);

    out_response->AddEncodingHeader(compression_type);
    MHD_add_response_header(*out_response, "Content-Type", "application/json");

    return 200;
}

}
