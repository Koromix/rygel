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
    http_IO *io = *(http_IO **)con_cls;

    // Avoid stale messages and messages from other theads in error pages
    ClearLastLogError();

    // Init request data
    if (!io) {
        io = new http_IO();
        *con_cls = io;

        io->request.conn = conn;
        io->request.method = method;

        // Trim URL prefix (base_url setting)
        for (Size i = 0; daemon->base_url[i]; i++, url++) {
            if (url[0] != daemon->base_url[i]) {
                if (!url[0] && daemon->base_url[i] == '/' && !daemon->base_url[i + 1]) {
                    io->AddHeader("Location", daemon->base_url);
                    return MHD_queue_response(conn, 303, io->response);
                } else {
                    http_ProduceErrorPage(404, io);
                    return MHD_queue_response(conn, (unsigned int)io->code, io->response);
                }
            }
        }
        io->request.url = --url;

        if (!NegociateContentEncoding(conn, &io->request.compression_type, io))
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
    }

    // There may be some kind of async runner
    std::unique_lock<std::mutex> lock(io->mutex);
    http_RequestInfo *request = &io->request;

    // Run first time handler
    if (io->state == http_IO::State::First) {
        daemon->handle_func(*request, io);

        if (io->state == http_IO::State::First) {
            io->state = http_IO::State::Done;
        }
    }

    // Process asynchronous tasks
    if (io->state == http_IO::State::Async) {
        io->Suspend();

        // Run async handler if needed
        if (io->async_func) {
            daemon->async->Run([=]() {
                std::function<void(const http_RequestInfo &request, http_IO *io)> func;
                std::swap(io->async_func, func);

                func(*request, io);

                std::unique_lock<std::mutex> lock(io->mutex);
                if (!io->async_func) {
                    io->state = http_IO::State::Done;
                }
                io->Resume();

                return true;
            });
        }

        return MHD_YES;
    } else if (io->state == http_IO::State::Read) {
        // Read upload data and give it to async handler
        if (*upload_data_size) {
            RG_ASSERT_DEBUG(io->read_buf.IsValid());

            Size copy_len = std::min(io->read_buf.len - io->read_len, (Size)*upload_data_size);

            memcpy(io->read_buf.ptr + io->read_len, upload_data, copy_len);
            io->read_len += copy_len;
            *upload_data_size -= copy_len;

            if (io->read_len == io->read_buf.len) {
                io->Suspend();
            }
        } else {
            io->state = http_IO::State::Async;
            io->Suspend();
        }

        io->read_cv.notify_one();

        return MHD_YES;
    }

    // We're done
    RG_ASSERT_DEBUG(io->state == http_IO::State::Done);
    if (io->code < 0) {
        // Default to internal error (if nothing else)
        http_ProduceErrorPage(500, io);
    }
    return MHD_queue_response(conn, (unsigned int)io->code, io->response);
}

void http_Daemon::RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                   MHD_RequestTerminationCode toe)
{
    http_IO *io = *(http_IO **)con_cls;

    if (io) {
        delete io;
    }
}

bool http_Daemon::Start(const http_Config &config,
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(!daemon);

    RG_ASSERT_DEBUG(config.base_url);
    RG_ASSERT_DEBUG(func);

    // Validate configuration
    {
        bool valid = true;

        if (config.port < 1 || config.port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", config.port, UINT16_MAX);
            valid = false;
        }
        if (config.threads <= 0 || config.threads > 128) {
            LogError("HTTP threads %1 is invalid (range: 1 - 128)", config.threads);
            valid = false;
        }
        if (config.async_threads <= 0) {
            LogError("HTTP async threads %1 is invalid (minimum: 1)", config.async_threads);
            valid = false;
        }
        if (config.base_url[0] != '/' || config.base_url[strlen(config.base_url) - 1] != '/') {
            LogError("Base URL '%1' does not start and end with '/'", config.base_url);
            valid = false;
        }

        if (!valid)
            return false;
    }

    // MHD options
    int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_ALLOW_SUSPEND_RESUME | MHD_USE_ERROR_LOG;
    LocalArray<MHD_OptionItem, 16> mhd_options;
    switch (config.ip_stack) {
        case IPStack::Dual: { flags |= MHD_USE_DUAL_STACK; } break;
        case IPStack::IPv4: {} break;
        case IPStack::IPv6: { flags |= MHD_USE_IPv6; } break;
    }
    if (config.threads > 1) {
        mhd_options.Append({MHD_OPTION_THREAD_POOL_SIZE, config.threads});
    }
    mhd_options.Append({MHD_OPTION_END, 0, nullptr});
#ifndef NDEBUG
    flags |= MHD_USE_DEBUG;
#endif

    handle_func = func;
    daemon = MHD_start_daemon(flags, (int16_t)config.port, nullptr, nullptr,
                              &http_Daemon::HandleRequest, this,
                              MHD_OPTION_NOTIFY_COMPLETED, &http_Daemon::RequestCompleted, this,
                              MHD_OPTION_ARRAY, mhd_options.data, MHD_OPTION_END);
    this->base_url = config.base_url;

    async = new Async(config.async_threads - 1);

    return daemon;
}

void http_Daemon::Stop()
{
    if (async) {
        async->Abort();
        delete async;
    }
    if (daemon) {
        MHD_stop_daemon(daemon);
    }

    async = nullptr;
    daemon = nullptr;
}

http_IO::http_IO()
{
    response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
}

http_IO::~http_IO()
{
    MHD_destroy_response(response);
}

void http_IO::RunAsync(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(state == State::First || state == State::Async);

    state = State::Async;
    async_func = func;
}

void http_IO::AddHeader(const char *key, const char *value)
{
    MHD_add_response_header(response, key, value);
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
    RG_ASSERT_DEBUG(new_code >= 0);

    code = new_code;

    MHD_move_response_headers(response, new_response);
    MHD_destroy_response(response);
    response = new_response;
}

bool http_IO::ReadPostValues(Allocator *alloc,
                             HashMap<const char *, const char *> *out_values)
{
    RG_ASSERT(state == State::Async);
    RG_ASSERT(TestStr(request.method, "POST"));

    struct PostProcessorContext {
        HashMap<const char *, const char *> *values;
        Allocator *alloc;
    };

    PostProcessorContext ctx = {};
    ctx.values = out_values;
    ctx.alloc = alloc;

    // Create POST data processor
    MHD_PostProcessor *pp =
        MHD_create_post_processor(request.conn, Kibibytes(32),
                                  [](void *cls, enum MHD_ValueKind, const char *key,
                                     const char *, const char *, const char *,
                                     const char *data, uint64_t, size_t) {
        PostProcessorContext *ctx = (PostProcessorContext *)cls;

        key = DuplicateString(key, ctx->alloc).ptr;
        data = DuplicateString(data, ctx->alloc).ptr;
        ctx->values->Append(key, data);

        return MHD_YES;
    }, &ctx);
    if (!pp) {
        LogError("Cannot parse this kind of POST data");
        http_ProduceErrorPage(422, this);
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex);

    // Start read
    state = State::Read;
    RG_DEFER_N(error_guard) {
        state = State::Async;
        Resume();
    };

    // Parse available upload data
    while (state == State::Read) {
        uint8_t buf[1024];
        read_buf = buf;
        read_len = 0;

        Resume();
        read_cv.wait(lock);

        if (MHD_post_process(pp, (const char *)buf, (size_t)read_len) != MHD_YES) {
            LogError("Failed to parse POST data");
            http_ProduceErrorPage(422, this);
            return false;
        }
    }

    error_guard.Disable();
    return true;
}

void http_IO::Suspend()
{
    if (!suspended) {
        MHD_suspend_connection(request.conn);
        suspended = true;
    }
}

void http_IO::Resume()
{
    if (suspended) {
        MHD_resume_connection(request.conn);
        suspended = false;
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

bool http_ProduceStaticAsset(Span<const uint8_t> data, CompressionType in_compression_type,
                             const char *mime_type, CompressionType out_compression_type,
                             http_IO *io)
{
    MHD_Response *response;
    if (in_compression_type != out_compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(data, nullptr, in_compression_type);
            StreamWriter writer(&buf, nullptr, out_compression_type);
            if (!SpliceStream(&reader, Megabytes(8), &writer))
                return false;
            if (!writer.Close())
                return false;
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

    return true;
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
