// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "http.hh"
#include "misc.hh"

namespace RG {

bool http_Daemon::Start(const http_Config &config,
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(!daemon);

    RG_ASSERT(config.base_url);
    RG_ASSERT(func);

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
    base_url = config.base_url;
    daemon = MHD_start_daemon(flags, (int16_t)config.port, nullptr, nullptr,
                              &http_Daemon::HandleRequest, this,
                              MHD_OPTION_NOTIFY_COMPLETED, &http_Daemon::RequestCompleted, this,
                              MHD_OPTION_ARRAY, mhd_options.data, MHD_OPTION_END);

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
        io->AttachError(406);
        return false;
    }
}

MHD_Result http_Daemon::HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                                      const char *, const char *upload_data, size_t *upload_data_size,
                                      void **con_cls)
{
    http_Daemon *daemon = (http_Daemon *)cls;
    http_IO *io = *(http_IO **)con_cls;

    bool first_call = !io;

    // Avoid stale messages and messages from other theads in error pages
    ClearLastLogError();

    // Init request data
    if (first_call) {
        io = new http_IO();
        *con_cls = io;

        io->daemon = daemon;
        io->request.base_url = daemon->base_url;
        io->request.conn = conn;
        io->request.method = method;

        // Trim URL prefix (base_url setting)
        for (Size i = 0; daemon->base_url[i]; i++, url++) {
            if (url[0] != daemon->base_url[i]) {
                if (!url[0] && daemon->base_url[i] == '/' && !daemon->base_url[i + 1]) {
                    io->AddHeader("Location", daemon->base_url);
                    return MHD_queue_response(conn, 303, io->response);
                } else {
                    io->AttachError(404);
                    return MHD_queue_response(conn, (unsigned int)io->code, io->response);
                }
            }
        }
        io->request.url = --url;

        if (!NegociateContentEncoding(conn, &io->request.compression_type, io))
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
    }

    // There may be some kind of async runner
    std::lock_guard<std::mutex> lock(io->mutex);
    http_RequestInfo *request = &io->request;

    // Run handler (sync first, and than async handlers if any)
    if (io->state == http_IO::State::Sync) {
        daemon->handle_func(*request, io);
        io->state = http_IO::State::Idle;
    }
    daemon->RunNextAsync(io);

    // Handle read/suspend while async handler is running
    if (io->state == http_IO::State::Async) {
        if (*upload_data_size) {
            if (io->read_len < io->read_buf.len) {
                // Read upload data and give it to async handler
                RG_ASSERT(io->read_buf.IsValid());

                Size copy_len = std::min(io->read_buf.len - io->read_len, (Size)*upload_data_size);

                memcpy(io->read_buf.ptr + io->read_len, upload_data, copy_len);
                io->read_len += copy_len;
                *upload_data_size -= copy_len;
            }
        } else {
            io->read_eof = !first_call;
        }

        // Try in all cases, even if not needed... too much spinning beats deadlock
        io->read_cv.notify_one();
    }

    // Handle write or attached response (if any)
    if (io->write_buf.len) {
        io->Resume();

        MHD_Response *new_response =
            MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, Kilobytes(16),
                                              &http_Daemon::HandleWrite, io, nullptr);
        MHD_move_response_headers(io->response, new_response);

        io->AttachResponse(io->write_code, new_response);
        io->AddEncodingHeader(request->compression_type);

        return MHD_queue_response(conn, (unsigned int)io->code, io->response);
    } else if (io->state == http_IO::State::Idle) {
        if (io->code < 0) {
            // Default to internal error (if nothing else)
            io->AttachError(500);
        }
        return MHD_queue_response(conn, (unsigned int)io->code, io->response);
    } else {
        // We must not suspend on first call because libmicrohttpd will call us back the same
        // way if we do so, with *upload_data_size = 0. Which means we'd have no reliable way
        // to differenciate between this first call and end of upload (request body).
        if (!first_call && io->read_len == io->read_buf.len) {
            io->Suspend();
        }
        return MHD_YES;
    }
}

ssize_t http_Daemon::HandleWrite(void *cls, uint64_t, char *buf, size_t max)
{
    http_IO *io = (http_IO *)cls;
    http_Daemon *daemon = io->daemon;

    std::unique_lock<std::mutex> lock(io->mutex);

    daemon->RunNextAsync(io);

    // Can't read anymore!
    RG_ASSERT(!io->read_buf.len);

    if (io->write_buf.len) {
        Size copy_len = std::min(io->write_buf.len - io->write_offset, (Size)max);
        memcpy(buf, io->write_buf.ptr + io->write_offset, copy_len);
        io->write_offset += copy_len;

        if (io->write_offset >= io->write_buf.len) {
            io->write_buf.RemoveFrom(0);
            io->write_offset = 0;

            io->write_cv.notify_one();
        }

        return copy_len;
    } else if (io->write_eof) {
        return MHD_CONTENT_READER_END_OF_STREAM;
    } else if (io->state != http_IO::State::Async) {
        // StreamWriter::Close() has not be closed, could be a late error
        LogError("Truncated HTTP response stream");
        return MHD_CONTENT_READER_END_WITH_ERROR;
    } else {
        // I tried to suspend here, but it triggered assert errors from libmicrohttpd,
        // and I don't know if it's not allowed, or if there's a bug. Need to investigate.
        return 0;
    }
}

// Call with io->mutex locked
void http_Daemon::RunNextAsync(http_IO *io)
{
    if (io->state == http_IO::State::Idle && io->async_func) {
        std::function<void()> func;
        std::swap(io->async_func, func);

        async->Run([=]() {
            func();

            std::unique_lock<std::mutex> lock(io->mutex);

            if (io->state == http_IO::State::Zombie) {
                lock.unlock();
                delete io;
            } else {
                io->state = http_IO::State::Idle;
                io->Resume();
            }

            return true;
        });

        io->state = http_IO::State::Async;
    }
}

void http_Daemon::RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                   MHD_RequestTerminationCode toe)
{
    http_IO *io = *(http_IO **)con_cls;

    if (io) {
        std::unique_lock<std::mutex> lock(io->mutex);

        if (io->state == http_IO::State::Async) {
            io->state = http_IO::State::Zombie;

            io->read_cv.notify_one();
            io->write_cv.notify_one();
        } else {
            lock.unlock();
            delete io;
        }
    }
}

http_IO::http_IO()
{
    response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
}

http_IO::~http_IO()
{
    for (const auto &func: finalizers) {
        func();
    }

    MHD_destroy_response(response);
}

void http_IO::RunAsync(std::function<void()> func)
{
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
    RG_ASSERT(max_age >= 0);

#ifndef NDEBUG
    max_age = 0;
#endif

    if (max_age || etag) {
        char buf[128];

        AddHeader("Cache-Control", Fmt(buf, "max-age=%1", max_age).ptr);
        if (etag) {
            AddHeader("ETag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

void http_IO::AttachResponse(int new_code, MHD_Response *new_response)
{
    RG_ASSERT(new_code >= 0);

    code = new_code;

    MHD_move_response_headers(response, new_response);
    MHD_destroy_response(response);
    response = new_response;
}

void http_IO::AttachText(int code, Span<const char> str, const char *mime_type)
{
    MHD_Response *response =
        MHD_create_response_from_buffer(str.len, (void *)str.ptr, MHD_RESPMEM_PERSISTENT);

    AttachResponse(code, response);
    AddHeader("Content-Type", mime_type);
}

bool http_IO::AttachBinary(int code, Span<const uint8_t> data, const char *mime_type,
                           CompressionType compression_type)
{
    MHD_Response *response;
    if (compression_type != request.compression_type) {
        HeapArray<uint8_t> buf;
        {
            StreamReader reader(data, nullptr, compression_type);
            StreamWriter writer(&buf, nullptr, request.compression_type);
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
    AttachResponse(code, response);

    AddEncodingHeader(request.compression_type);
    if (mime_type) {
        AddHeader("Content-Type", mime_type);
    }

    return true;
}

void http_IO::AttachError(int code, const char *details)
{
    Span<char> page = Fmt((Allocator *)nullptr, "Error %1: %2\n%3", code,
                          MHD_get_reason_phrase_for((unsigned int)code), details ? details : "");

    MHD_Response *response =
        MHD_create_response_from_buffer_with_free_callback((size_t)page.len, page.ptr,
                                                           ReleaseDataCallback);
    AttachResponse(code, response);
    AddHeader("Content-Type", "text/plain");
}

bool http_IO::OpenForRead(StreamReader *out_st)
{
    RG_ASSERT(state != State::Sync);

    return out_st->Open([this](Span<uint8_t> out_buf) { return Read(out_buf); }, "<http>");
}

bool http_IO::OpenForWrite(int code, StreamWriter *out_st)
{
    RG_ASSERT(state != State::Sync);

    write_code = code;
    return out_st->Open([this](Span<const uint8_t> buf) { return Write(buf); }, "<http>",
                        request.compression_type);
}

bool http_IO::ReadPostValues(Allocator *alloc,
                             HashMap<const char *, const char *> *out_values)
{
    RG_ASSERT(state != State::Sync);
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
        ctx->values->Set(key, data);

        return MHD_YES;
    }, &ctx);
    if (!pp) {
        LogError("Cannot parse this kind of POST data");
        return false;
    }
    RG_DEFER { MHD_destroy_post_processor(pp); };

    // Parse available upload data
    {
        Size total_len = 0;
        for (;;) {
            LocalArray<uint8_t, 1024> buf;
            buf.len = Read(buf.data);
            if (buf.len < 0) {
                return false;
            } else if (!buf.len) {
                break;
            }

            if (RG_UNLIKELY(buf.len > Kibibytes(32) - total_len)) {
                LogError("POST body is too long (max: %1)", FmtMemSize(buf.len));
                return false;
            }
            total_len += buf.len;

            if (MHD_post_process(pp, (const char *)buf.data, (size_t)buf.len) != MHD_YES) {
                LogError("Failed to parse POST data");
                return false;
            }
        }
    }

    return true;
}

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    finalizers.Append(func);
}

Size http_IO::Read(Span<uint8_t> out_buf)
{
    RG_ASSERT(state != State::Sync);

    std::unique_lock<std::mutex> lock(mutex);

    // Set read buffer
    read_buf = out_buf;
    read_len = 0;
    RG_DEFER {
        read_buf = {};
        read_len = 0;
    };

    // Wait for libmicrohttpd
    while (state == State::Async && !read_len && !read_eof) {
        Resume();
        read_cv.wait(lock);
    }
    if (state == State::Zombie) {
        LogError("Connection aborted");
        return -1;
    }

    return read_len;
}

bool http_IO::Write(Span<const uint8_t> buf)
{
    RG_ASSERT(state != State::Sync);
    RG_ASSERT(!write_eof);

    std::unique_lock<std::mutex> lock(mutex);

    // Make sure we switch to write state
    Resume();

    write_eof |= !buf.len;
    while (state == State::Async && write_buf.len >= Kilobytes(4)) {
        write_cv.wait(lock);
    }
    write_buf.Append(buf);

    if (state == State::Zombie) {
        LogError("Connection aborted");
        return false;
    }

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

}
