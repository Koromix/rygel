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
#include "http.hh"
#include "http_misc.hh"
#include "../../../vendor/mbedtls/include/mbedtls/sha1.h"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifdef _WIN32
    #include <io.h>
    #include <ws2tcpip.h>

    #ifndef UNIX_PATH_MAX
        #define UNIX_PATH_MAX 108
    #endif
    typedef struct sockaddr_un {
        ADDRESS_FAMILY sun_family;
        char sun_path[UNIX_PATH_MAX];
    } SOCKADDR_UN, *PSOCKADDR_UN;
#else
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <sys/uio.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <unistd.h>
#endif

namespace RG {

bool http_Config::Validate() const
{
    bool valid = true;

    if (sock_type == SocketType::Unix) {
        struct sockaddr_un addr;

        if (!unix_path) {
            LogError("Unix socket path must be set");
            valid = false;
        }
        if (strlen(unix_path) >= sizeof(addr.sun_path)) {
            LogError("Socket path '%1' is too long (max length = %2)", unix_path, sizeof(addr.sun_path) - 1);
            valid = false;
        }
    } else if (port < 1 || port > UINT16_MAX) {
        LogError("HTTP port %1 is invalid (range: 1 - %2)", port, UINT16_MAX);
        valid = false;
    }
    if (max_connections < 0) {
        LogError("HTTP max connections cannot be negative (%1)", max_connections);
        valid = false;
    }
    if (idle_timeout < 0) {
        LogError("HTTP idle timeout cannot be negative (%1)", idle_timeout);
        valid = false;
    }
    if (threads <= 0 || threads > 128) {
        LogError("HTTP threads %1 is invalid (range: 1 - 128)", threads);
        valid = false;
    }
    if (async_threads <= 0) {
        LogError("HTTP async threads %1 is invalid (minimum: 1)", async_threads);
        valid = false;
    }

    return valid;
}

bool http_Daemon::Bind(const http_Config &config)
{
    RG_ASSERT(!daemon);
    RG_ASSERT(listen_fd < 0);

    // Validate configuration
    if (!config.Validate())
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listen_fd = OpenIPSocket(config.sock_type, config.port); } break;
        case SocketType::Unix: { listen_fd = OpenUnixSocket(config.unix_path); } break;
    }
    if (listen_fd < 0)
        return false;

    if (listen(listen_fd, 1024) < 0) {
        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    return true;
}

bool http_Daemon::Start(const http_Config &config,
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(!daemon);
    RG_ASSERT(func);

    // Validate configuration
    if (!config.Validate())
        return false;

    if (config.client_addr_mode == http_ClientAddressMode::Socket) {
        LogInfo("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-IP "
                "if you run this behind a reverse proxy that sets one of these headers.");
    }

    // Prepare socket (if not done yet)
    if (listen_fd < 0 && !Bind(config))
        return false;

    // MHD flags
    int flags = MHD_USE_AUTO_INTERNAL_THREAD | MHD_ALLOW_SUSPEND_RESUME |
                MHD_ALLOW_UPGRADE | MHD_USE_ERROR_LOG;
#ifndef NDEBUG
    flags |= MHD_USE_DEBUG;
#endif

    // MHD options
    LocalArray<MHD_OptionItem, 16> mhd_options;
    mhd_options.Append({MHD_OPTION_LISTEN_SOCKET, listen_fd});
    if (config.threads > 1) {
        mhd_options.Append({MHD_OPTION_THREAD_POOL_SIZE, config.threads});
    }
    if (config.max_connections) {
        mhd_options.Append({MHD_OPTION_CONNECTION_LIMIT, config.max_connections});
    }
    mhd_options.Append({MHD_OPTION_CONNECTION_TIMEOUT, config.idle_timeout});
    mhd_options.Append({MHD_OPTION_END, 0, nullptr});
    client_addr_mode = config.client_addr_mode;

#ifdef _WIN32
    stop_handle = WSACreateEvent();
    if (!stop_handle) {
        LogError("CreateEvent() failed: %1", GetWin32ErrorString());
        return false;
    }
#else
    if (!CreatePipe(stop_pfd))
        return false;
#endif

    handle_func = func;
    async = new Async(config.async_threads - 1);

    running = true;
    daemon = MHD_start_daemon(flags, 0, nullptr, nullptr,
                              &http_Daemon::HandleRequest, this,
                              MHD_OPTION_NOTIFY_COMPLETED, &http_Daemon::RequestCompleted, this,
                              MHD_OPTION_ARRAY, mhd_options.data, MHD_OPTION_END);

    return daemon;
}

void http_Daemon::Stop()
{
    running = false;

    if (async) {
#ifdef _WIN32
        WSASetEvent(stop_handle);

        async->Sync();
        delete async;

        WSACloseEvent(stop_handle);
#else
        char dummy = 0;
        RG_IGNORE write(stop_pfd[1], &dummy, 1);

        async->Sync();
        delete async;

        close(stop_pfd[0]);
        close(stop_pfd[1]);
#endif
    }

    if (daemon) {
        MHD_stop_daemon(daemon);
    } else if (listen_fd >= 0) {
        CloseSocket(listen_fd);
    }
    listen_fd = -1;

    async = nullptr;
    daemon = nullptr;
}

static bool GetClientAddress(MHD_Connection *conn, http_ClientAddressMode addr_mode, Span<char> out_address)
{
    RG_ASSERT(out_address.len);

    switch (addr_mode) {
        case http_ClientAddressMode::Socket: {
            int family;
            void *addr;
            {
                sockaddr *saddr =
                    MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;

                family = saddr->sa_family;
                switch (saddr->sa_family) {
                    case AF_INET: { addr = &((sockaddr_in *)saddr)->sin_addr; } break;
                    case AF_INET6: { addr = &((sockaddr_in6 *)saddr)->sin6_addr; } break;
#ifndef _WIN32
                    case AF_UNIX: {
                        CopyString("unix", out_address);
                        return true;
                    } break;
#endif

                    default: { RG_UNREACHABLE(); } break;
                }
            }

            if (!inet_ntop(family, addr, out_address.ptr, out_address.len)) {
                LogError("Cannot convert network address to text");
                return false;
            }

            return true;
        } break;

        case http_ClientAddressMode::XForwardedFor: {
            const char *str = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "X-Forwarded-For");
            if (!str) {
                LogError("X-Forwarded-For header is missing but is required by the configuration");
                return false;
            }

            Span<const char> addr = TrimStr(SplitStr(str, ','));

            if (RG_UNLIKELY(!addr.len)) {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (RG_UNLIKELY(!CopyString(addr, out_address))) {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }

            return true;
        } break;

        case http_ClientAddressMode::XRealIP: {
            const char *str = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "X-Real-IP");
            if (!str) {
                LogError("X-Real-IP header is missing but is required by the configuration");
                return false;
            }

            Span<const char> addr = TrimStr(str);

            if (RG_UNLIKELY(!addr.len)) {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (RG_UNLIKELY(!CopyString(addr, out_address))) {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }

            return true;
        } break;
    }

    RG_UNREACHABLE();
}

MHD_Result http_Daemon::HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                                      const char *, const char *upload_data, size_t *upload_data_size,
                                      void **con_cls)
{
    http_Daemon *daemon = (http_Daemon *)cls;
    http_IO *io = *(http_IO **)con_cls;

    if (RG_UNLIKELY(!daemon->running)) {
        const char *msg = "Server is shutting down";

        MHD_Response *response = MHD_create_response_from_buffer(strlen(msg), (void *)msg, MHD_RESPMEM_PERSISTENT);
        RG_DEFER { MHD_destroy_response(response); };

        return MHD_queue_response(conn, 503, response);
    }

    bool first_call = !io;

    // Init request data
    if (first_call) {
        io = new http_IO;
        *con_cls = io;

        io->daemon = daemon;
        io->request.conn = conn;
        io->request.url = url;

        // Is that even possible? Dunno, but make sure it never happens!
        if (RG_UNLIKELY(url[0] != '/')) {
            io->AttachError(400);
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
        }

        if (TestStr(method, "HEAD")) {
            io->request.method = http_RequestMethod::Get;
            io->request.headers_only = true;
        } else if (!OptionToEnum(http_RequestMethodNames, method, &io->request.method)) {
            io->AttachError(405);
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
        }
        if (!GetClientAddress(conn, daemon->client_addr_mode, io->request.client_addr)) {
            io->AttachError(422);
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
        }
    }

    // There may be some kind of async runner
    std::lock_guard<std::mutex> lock(io->mutex);
    http_RequestInfo *request = &io->request;

    io->PushLogFilter();
    RG_DEFER { PopLogFilter(); };

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

                memcpy_safe(io->read_buf.ptr + io->read_len, upload_data, copy_len);
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
    if (io->force_queue) {
        io->Resume();
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
        memcpy_safe(buf, io->write_buf.ptr + io->write_offset, copy_len);
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

        async->Run([=, this]() {
            io->PushLogFilter();
            RG_DEFER { PopLogFilter(); };

            if (RG_LIKELY(running)) {
                func();
            }

            std::unique_lock<std::mutex> lock(io->mutex);

            if (io->state == http_IO::State::Zombie) {
                lock.unlock();
                delete io;
            } else {
                if (io->ws_urh && !io->async_func) {
                    MHD_upgrade_action(io->ws_urh, MHD_UPGRADE_ACTION_CLOSE);
                    io->suspended = false;
                }

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

        if (io->state == http_IO::State::Async || io->state == http_IO::State::WebSocket) {
            io->state = http_IO::State::Zombie;

            if (io->ws_urh) {
                MHD_upgrade_action(io->ws_urh, MHD_UPGRADE_ACTION_CLOSE);
            }

            io->read_cv.notify_one();
            io->write_cv.notify_one();
            io->ws_cv.notify_one();
        } else {
            lock.unlock();
            delete io;
        }
    }
}

http_IO::http_IO()
{
    ResetResponse();
}

http_IO::~http_IO()
{
    for (const auto &func: finalizers) {
        func();
    }

#ifdef _WIN32
    if (ws_handle) {
        WSACloseEvent(ws_handle);
    }
#endif

    MHD_destroy_response(response);
}

bool http_IO::NegociateEncoding(CompressionType preferred, CompressionType *out_encoding)
{
    const char *accept_str = request.GetHeaderValue("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)preferred)) {
        *out_encoding = preferred;
        return true;
    } else if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    } else {
        AttachError(406);
        return false;
    }
}

bool http_IO::NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding)
{
    const char *accept_str = request.GetHeaderValue("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)preferred1)) {
        *out_encoding = preferred1;
        return true;
    } else if (acceptable_encodings & (1 << (int)preferred2)) {
        *out_encoding = preferred2;
        return true;
    } else if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    } else {
        AttachError(406);
        return false;
    }
}

void http_IO::RunAsync(std::function<void()> func)
{
    async_func = func;
    async_func_response = false;
}

void http_IO::AddHeader(const char *key, const char *value)
{
    MHD_add_response_header(response, key, value);
}

void http_IO::AddEncodingHeader(CompressionType encoding)
{
    switch (encoding) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: { AddHeader("Content-Encoding", "deflate"); } break;
        case CompressionType::Gzip: { AddHeader("Content-Encoding", "gzip"); } break;
        case CompressionType::Brotli: { AddHeader("Content-Encoding", "br"); } break;
    }
}

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value,
                              bool http_only)
{
    LocalArray<char, 1024> buf;

    if (value) {
        buf.len = Fmt(buf.data, "%1=%2; Path=%3;", name, value, path).len;
    } else {
        buf.len = Fmt(buf.data, "%1=; Path=%2; Max-Age=0;", name, path).len;
    }

    RG_ASSERT(buf.Available() >= 64);
    buf.len += Fmt(buf.TakeAvailable(), " SameSite=Strict;%1", http_only ? " HttpOnly;" : "").len;

    AddHeader("Set-Cookie", buf.data);
}

void http_IO::AddCachingHeaders(int max_age, const char *etag)
{
    RG_ASSERT(max_age >= 0);

#ifndef NDEBUG
    max_age = 0;
#endif

    if (max_age || etag) {
        char buf[128];

        AddHeader("Cache-Control", max_age ? Fmt(buf, "max-age=%1", max_age).ptr : "no-store");
        if (etag) {
            AddHeader("ETag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

void http_IO::ResetResponse()
{
    code = -1;

    MHD_destroy_response(response);
    response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
}

void http_IO::AttachResponse(int new_code, MHD_Response *new_response)
{
    RG_ASSERT(new_code >= 0);

    code = new_code;

    MHD_move_response_headers(response, new_response);
    MHD_destroy_response(response);
    response = new_response;

    if (async_func_response) {
        async_func = {};
        async_func_response = false;
    }
}

void http_IO::AttachText(int code, Span<const char> str, const char *mime_type)
{
    MHD_Response *response =
        MHD_create_response_from_buffer(str.len, (void *)str.ptr, MHD_RESPMEM_PERSISTENT);

    AttachResponse(code, response);
    AddHeader("Content-Type", mime_type);
}

bool http_IO::AttachBinary(int code, Span<const uint8_t> data, const char *mime_type,
                           CompressionType src_encoding)
{
    CompressionType dest_encoding;
    if (!NegociateEncoding(src_encoding, &dest_encoding))
        return false;

    if (dest_encoding != src_encoding) {
        if (request.headers_only) {
            AttachNothing(code);
            AddEncodingHeader(dest_encoding);
        } else {
            RunAsync([=, this]() {
                StreamReader reader(data, nullptr, src_encoding);

                StreamWriter writer;
                if (!OpenForWrite(code, -1, dest_encoding, &writer))
                    return;
                AddEncodingHeader(dest_encoding);

                if (!SpliceStream(&reader, Megabytes(8), &writer))
                    return;
                writer.Close();
            });
            async_func_response = true;
        }
    } else {
        MHD_Response *response =
            MHD_create_response_from_buffer((size_t)data.len, (void *)data.ptr, MHD_RESPMEM_PERSISTENT);
        AttachResponse(code, response);
        AddEncodingHeader(dest_encoding);
    }

    if (mime_type) {
        AddHeader("Content-Type", mime_type);
    }

    return true;
}

void http_IO::AttachError(int code, const char *details)
{
    if (!details) {
        details = (code < 500 && last_err) ? last_err : "";
    }

    Span<char> page = Fmt(&allocator, "Error %1: %2\n%3", code,
                          MHD_get_reason_phrase_for((unsigned int)code), details);

    MHD_Response *response = MHD_create_response_from_buffer((size_t)page.len, page.ptr, MHD_RESPMEM_PERSISTENT);
    AttachResponse(code, response);

    AddHeader("Content-Type", "text/plain");
}

bool http_IO::AttachFile(int code, const char *filename)
{
    FileInfo file_info;
    if (!StatFile(filename, &file_info))
        return false;

    int fd = OpenDescriptor(filename, (int)OpenFileFlag::Read | (int)OpenFileFlag::Unlinkable);
    if (fd < 0)
        return false;

    MHD_Response *response = MHD_create_response_from_fd((uint64_t)file_info.size, fd);
    AttachResponse(code, response);

    return true;
}

void http_IO::AttachNothing(int code)
{
    // We don't want libmicrohttpd to send Content-Length, so we use a callback response
    const auto null_callback = [](void *, uint64_t, char *, size_t) {
        return (ssize_t)MHD_CONTENT_READER_END_OF_STREAM;
    };

    MHD_Response *response =
        MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, Kilobytes(16), null_callback, nullptr, nullptr);
    AttachResponse(code, response);
}

bool http_IO::OpenForRead(Size max_len, StreamReader *out_st)
{
    RG_ASSERT(state != State::Sync && state != State::WebSocket);

    if (max_len >= 0) {
        if (const char *str = request.GetHeaderValue("Content-Length"); str) {
            Size len;
            if (RG_UNLIKELY(!ParseInt(str, &len))) {
                AttachError(400);
                return false;
            }
            if (RG_UNLIKELY(len < 0)) {
                LogError("Refusing negative Content-Length");
                AttachError(400);
                return  false;
            }

            if (len > max_len) {
                LogError("HTTP body is too big (max = %1)", FmtDiskSize(max_len));
                AttachError(413);
                return false;
            }
        }
    }

    read_max = max_len;

    bool success = out_st->Open([this](Span<uint8_t> out_buf) { return Read(out_buf); }, "<http>");
    RG_ASSERT(success);

    return true;
}

bool http_IO::OpenForWrite(int code, Size len, CompressionType encoding, StreamWriter *out_st)
{
    RG_ASSERT(state != State::Sync && state != State::WebSocket);

    write_code = code;
    write_len = (len >= 0) ? (uint64_t)len : MHD_SIZE_UNKNOWN;

    bool success = out_st->Open([this](Span<const uint8_t> buf) { return Write(buf); }, "<http>", encoding);
    return success;
}

bool http_IO::ReadPostValues(Allocator *alloc, HashMap<const char *, const char *> *out_values)
{
    RG_ASSERT(state != State::Sync);
    RG_ASSERT(request.method == http_RequestMethod::Post);
    RG_ASSERT(alloc);

    struct PostProcessorContext {
        HashMap<const char *, const char *> *values;
        const char *key;
        HeapArray<char> buf;
    };

    PostProcessorContext ctx = {};
    ctx.values = out_values;
    ctx.buf.allocator = alloc;

    // Create POST data processor
    MHD_PostProcessor *pp =
        MHD_create_post_processor(request.conn, Kibibytes(32),
                                  [](void *cls, enum MHD_ValueKind, const char *key,
                                     const char *, const char *, const char *,
                                     const char *data, uint64_t offset, size_t size) {
        PostProcessorContext *ctx = (PostProcessorContext *)cls;

        if (!ctx->key) {
            ctx->key = DuplicateString(key, ctx->buf.allocator).ptr;
        } else if (!TestStr(key, ctx->key)) {
            ctx->buf.Append(0);

            const char *value = ctx->buf.TrimAndLeak().ptr;
            ctx->values->Set(ctx->key, value);

            ctx->key = DuplicateString(key, ctx->buf.allocator).ptr;
        }

        RG_ASSERT(offset == (uint64_t)ctx->buf.len);
        ctx->buf.Append(MakeSpan(data, (Size)size));

        return MHD_YES;
    }, &ctx);
    if (!pp) {
        LogError("Cannot parse this kind of POST data");
        return false;
    }
    RG_DEFER { MHD_destroy_post_processor(pp); };

    read_max = Kibibytes(32);

    // Parse available upload data
    for (;;) {
        LocalArray<uint8_t, 1024> buf;
        buf.len = Read(buf.data);
        if (buf.len < 0) {
            return false;
        } else if (!buf.len) {
            break;
        }

        if (MHD_post_process(pp, (const char *)buf.data, (size_t)buf.len) != MHD_YES) {
            LogError("Failed to parse POST data");
            return false;
        }
    }

    // This may trigger a new call to the iterator callback with remaining buffered
    // data, so we really need to do this before we handle our own leftovers.
    MHD_destroy_post_processor(pp);
    pp = nullptr;

    if (ctx.key) {
        ctx.buf.Append(0);

        const char *value = ctx.buf.TrimAndLeak().ptr;
        out_values->Set(ctx.key, value);
    }

    return true;
}

void http_IO::HandleUpgrade(void *cls, struct MHD_Connection *, void *,
                            const char *extra_in, size_t extra_in_size, MHD_socket fd,
                            struct MHD_UpgradeResponseHandle *urh)
{
    http_IO *io = (http_IO *)cls;

    // Notify when handler ends
    RG_DEFER_N(err_guard) {
        MHD_upgrade_action(io->ws_urh, MHD_UPGRADE_ACTION_CLOSE);
        io->ws_cv.notify_one();
    };

    std::lock_guard<std::mutex> lock(io->mutex);

    // Set non-blocking socket behavior
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) < 0) {
        LogError("Failed to make socket non-blocking: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
#else
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        LogError("Failed to make socket non-blocking: %1", strerror(errno));
        return;
    }
    if (!(flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LogError("Failed to make socket non-blocking: %1", strerror(errno));
        return;
    }
#endif

    io->ws_urh = urh;
    io->ws_fd = fd;
    io->ws_buf.Append(MakeSpan((const uint8_t *)extra_in, (Size)extra_in_size));
    io->ws_offset = 0;

#ifdef _WIN32
    io->ws_handle = WSACreateEvent();
    if (!io->ws_handle) {
        LogError("WSACreateEvent() failed: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
    if (WSAEventSelect(fd, io->ws_handle, FD_READ | FD_CLOSE)) {
        LogError("Failed to associate event with socket: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
#endif

    err_guard.Disable();
    io->state = State::WebSocket;
    io->ws_cv.notify_one();
}

bool http_IO::IsWS() const
{
    const char *conn_str = request.GetHeaderValue("Connection");
    const char *upgrade_str = request.GetHeaderValue("Upgrade");

    if (!conn_str || !strstr(conn_str, "Upgrade"))
        return false;
    if (!upgrade_str || !TestStr(upgrade_str, "websocket"))
        return false;

    return true;
}

bool http_IO::UpgradeToWS(unsigned int flags)
{
    RG_ASSERT(state != State::Sync && state != State::WebSocket);
    RG_ASSERT(!force_queue);

    if (!IsWS()) {
        LogError("Missing mandatory WebSocket headers");
        AttachError(400);
        return false;
    }

    // Check WebSocket headers
    const char *key_str;
    {
        const char *version_str = request.GetHeaderValue("Sec-WebSocket-Version");
        key_str = request.GetHeaderValue("Sec-WebSocket-Key");

        if (!version_str || !TestStr(version_str, "13")) {
            LogError("Unsupported Websocket version '%1'", version_str);
            AddHeader("Sec-WebSocket-Version", "13");
            AttachError(426);
            return false;
        }
        if (!key_str) {
            LogError("Missing 'Sec-WebSocket-Key' header");
            AttachError(400);
            return false;
        }
    }

    // Compute accept value. Who designed this?
    char accept_str[128];
    {
        LocalArray<char, 128> full_key;
        uint8_t hash[20];

        full_key.len = Fmt(full_key.data, "%1%2", key_str, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").len;
        mbedtls_sha1((const uint8_t *)full_key.data, full_key.len, hash);
        sodium_bin2base64(accept_str, RG_SIZE(accept_str), hash, RG_SIZE(hash), sodium_base64_VARIANT_ORIGINAL);
    }

    MHD_Response *response = MHD_create_response_for_upgrade(HandleUpgrade, this);
    AttachResponse(101, response);

    AddHeader("Upgrade", "websocket");
    AddHeader("Sec-WebSocket-Accept", accept_str);

    ws_opcode = (flags & (int)http_WebSocketFlag::Text) ? 1 : 2;

    // Wait for the handler to run
    {
        std::unique_lock<std::mutex> lock(mutex);

        force_queue = true;
        while (!ws_urh) {
            if (!daemon->running) {
                LogError("Server is shutting down");
                return false;
            }
            if (state == State::Zombie) {
                LogError("Lost connection during WebSocket upgrade");
                return false;
            }

            Resume();
            ws_cv.wait(lock);
        }
        if (state != State::WebSocket)
            return false;
    }

    return true;
}

void http_IO::OpenForReadWS(StreamReader *out_st)
{
    out_st->Open([this](Span<uint8_t> out_buf) { return ReadWS(out_buf); }, "<ws>");
}

bool http_IO::OpenForWriteWS(CompressionType encoding, StreamWriter *out_st)
{
    bool success = out_st->Open([this](Span<const uint8_t> buf) { return WriteWS(buf); }, "<ws>", encoding);
    return success;
}

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    finalizers.Append(func);
}

void http_IO::PushLogFilter()
{
    // This log filter does two things: it keeps a copy of the last log error message,
    // and it sets the log context to the client address (for log file).
    RG::PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        if (level == LogLevel::Error) {
            last_err = DuplicateString(msg, &allocator).ptr;
        }

        if (ctx) {
            char ctx_buf[512];
            Fmt(ctx_buf, "%1: %2", request.client_addr, ctx);

            func(level, ctx_buf, msg);
        } else {
            func(level, request.client_addr, msg);
        }
    });
}

Size http_IO::Read(Span<uint8_t> out_buf)
{
    std::unique_lock<std::mutex> lock(mutex);

    RG_ASSERT(state != State::Sync);

    // Set read buffer
    read_buf = out_buf;
    read_len = 0;
    RG_DEFER {
        read_buf = {};
        read_len = 0;
    };

    // Wait for libmicrohttpd
    while (state == State::Async && !read_len && !read_eof) {
        if (!daemon->running) {
            LogError("Server is shutting down");
            return false;
        }

        Resume();
        read_cv.wait(lock);
    }
    if (state == State::Zombie) {
        LogError("Connection aborted while reading");
        return -1;
    }

    if (RG_UNLIKELY(read_max >= 0 && read_len > read_max - read_total)) {
        LogError("HTTP body is too big (max = %1)", FmtDiskSize(read_max));
        AttachError(413);
        return -1;
    }
    read_total += read_len;

    return read_len;
}

bool http_IO::Write(Span<const uint8_t> buf)
{
    std::unique_lock<std::mutex> lock(mutex);

    RG_ASSERT(state != State::Sync);
    RG_ASSERT(!write_eof);

    if (!force_queue) {
        MHD_Response *new_response =
            MHD_create_response_from_callback(write_len, Kilobytes(16),
                                              &http_Daemon::HandleWrite, this, nullptr);
        AttachResponse(write_code, new_response);

        force_queue = true;
    }

    // Make sure we switch to write state
    Resume();

    write_eof |= !buf.len;
    while (state == State::Async && write_buf.len >= Kilobytes(4)) {
        if (!daemon->running) {
            LogError("Server is shutting down");
            return false;
        }

        write_cv.wait(lock);
    }
    write_buf.Append(buf);

    if (!write_eof && state == State::Zombie) {
        LogError("Connection aborted while writing");
        return false;
    }

    return true;
}

Size http_IO::ReadWS(Span<uint8_t> out_buf)
{
#ifndef NDEBUG
    {
        std::unique_lock<std::mutex> lock(mutex);
        RG_ASSERT(state == State::WebSocket || state == State::Zombie);
    }
#endif

    bool begin = false;
    Size read_len = 0;

    while (out_buf.len) {
        // Check status
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (state == State::Zombie)
                break;
        }

        // Decode message
        {
            if (ws_buf.len < 2)
                goto pump;

            int bits = (ws_buf[0] >> 4) & 0xF;
            int opcode = ws_buf[0] & 0xF;
            bool fin = bits & 0xF;

            if (opcode == 1 || opcode == 2) {
                begin = true;
                read_len = 0;
            } else if (opcode == 8) {
                return 0;
            }
            begin &= (opcode < 3);

            bool masked = ws_buf[1] & 0x80;
            Size payload = ws_buf[1] & 0x7F;

            if (bits != 8 && bits != 0) {
                LogError("Unsupported WebSocket RSV bits");
                return -1;
            }
            if (!masked) {
                LogError("Client to server messages must be masked");
                return -1;
            }

            Size offset;
            uint8_t mask[4];
            if (payload == 0x7E) {
                if (ws_buf.len < 8)
                    goto pump;

                uint16_t payload16;
                memcpy_safe(&payload16, ws_buf.ptr + 2, 2);
                memcpy_safe(mask, ws_buf.ptr + 4, 4);

                payload = BigEndian(payload16);
                offset = 8;
            } else if (payload == 0x7F) {
                if (ws_buf.len < 14)
                    goto pump;

                memcpy_safe(&payload, ws_buf.ptr + 2, 8);
                memcpy_safe(mask, ws_buf.ptr + 10, 4);

                payload = BigEndian(payload);
                offset = 14;
            } else {
                if (ws_buf.len < 6)
                    goto pump;

                memcpy_safe(mask, ws_buf.ptr + 2, 4);

                offset = 6;
            }
            if (ws_buf.len - offset < payload)
                goto pump;

            if (begin) {
                Size avail_len = std::min(payload, ws_buf.len - offset);
                Size copy_len = std::min(out_buf.len - read_len, avail_len);

                for (Size i = 0; i < copy_len; i++) {
                    out_buf[read_len++] = ws_buf[offset + i] ^ mask[i % 4];
                }
            }

            ws_buf.len = std::max(ws_buf.len - offset - payload, (Size)0);
            memmove_safe(ws_buf.ptr, ws_buf.ptr + offset + payload, ws_buf.len);

            // We can't return empty messages because this is a signal for EOF
            // in the StreamReader code. Oups.
            if (begin && fin && read_len)
                break;
        }

pump:
        // Pump more data from the OS
        ws_buf.Grow(Kibibytes(1));

#ifdef _WIN32
        void *events[2] = {
            ws_handle,
            daemon->stop_handle
        };

        if (WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE) == WSA_WAIT_FAILED) {
            LogError("Failed to read from socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return -1;
        }
        WSAResetEvent(ws_handle);
#else
        struct pollfd pfds[2] = {
            {ws_fd, POLLIN},
            {daemon->stop_pfd[0], POLLIN}
        };

        if (poll(pfds, RG_LEN(pfds), -1) < 0) {
            LogError("Failed to read from socket: %1", strerror(errno));
            return -1;
        }
#endif

        if (RG_UNLIKELY(!daemon->running)) {
            LogError("Server is shutting down");
            return -1;
        }

        ssize_t len = recv(ws_fd, (char *)ws_buf.end(), (int)(ws_buf.capacity - ws_buf.len), 0);
        if (len < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                continue;

#ifdef _WIN32
            LogError("Failed to read from socket: %1", GetWin32ErrorString(WSAGetLastError()));
#else
            LogError("Failed to read from socket: %1", strerror(errno));
#endif
            return -1;
        } else if (!len) {
            break;
        }
        ws_buf.len += (Size)len;
    }

    return read_len;
}

bool http_IO::WriteWS(Span<const uint8_t> buf)
{
#ifndef NDEBUG
    {
        std::unique_lock<std::mutex> lock(mutex);
        RG_ASSERT(state == State::WebSocket || state == State::Zombie);
    }
#endif

    int opcode = ws_opcode;

    while (buf.len) {
        // Check status
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (state == State::Zombie)
                break;
        }

        Size part_len = std::min(buf.len, (Size)4096 - 4);
        Span<const uint8_t> part = buf.Take(0, part_len);

        buf = buf.Take(part_len, buf.len - part_len);

        LocalArray<uint8_t, 4> frame;
        frame.data[0] = (uint8_t)((buf.len ? 0 : 0x8 << 4) | opcode);
        frame.data[1] = (uint8_t)std::min(part_len, (Size)126);
        if (part_len >= 126) {
            frame.data[2] = (uint8_t)(part_len >> 8);
            frame.data[3] = (uint8_t)(part_len & 0xFF);
            frame.len = 4;
        } else {
            frame.len = 2;
        }
        opcode = 0;

#ifdef _WIN32
        if (send(ws_fd, (const char *)frame.data, (int)frame.len, 0) < 0) {
            LogError("Failed to write to socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return false;
        }
        if (send(ws_fd, (const char *)part.ptr, (int)part.len, 0) < 0) {
            LogError("Failed to write to socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return false;
        }
#else
        struct iovec iov[2] = {
            {(void *)frame.data, (size_t)frame.len},
            {(void *)part.ptr, (size_t)part.len}
        };

        if (writev(ws_fd, iov, RG_LEN(iov)) < 0) {
            LogError("Failed to write to socket: %1", strerror(errno));
            return false;
        }
#endif
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
