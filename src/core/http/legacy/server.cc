// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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

#if defined(_WIN32)
    #include <io.h>
    #include <ws2tcpip.h>

    #if !defined(UNIX_PATH_MAX)
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
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <poll.h>
#endif

namespace RG {

bool http_Config::SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory)
{
    if (key == "SocketType" || key == "IPStack") {
        if (!OptionToEnumI(SocketTypeNames, value, &sock_type)) {
            LogError("Unknown socket type '%1'", value);
            return false;
        }

        return true;
    } else if (key == "UnixPath") {
        unix_path = NormalizePath(value, root_directory, &str_alloc).ptr;
        return true;
    } else if (key == "Port") {
        return ParseInt(value, &port);
    } else if (key == "MaxConnections") {
        return ParseInt(value, &max_connections);
    } else if (key == "IdleTimeout") {
        return ParseDuration(value, &idle_timeout);
    } else if (key == "Threads") {
        return ParseInt(value, &threads);
    } else if (key == "AsyncThreads") {
        return ParseInt(value, &async_threads);
    } else if (key == "ClientAddress") {
        if (!OptionToEnumI(http_ClientAddressModeNames, value, &client_addr_mode)) {
            LogError("Unknown client address mode '%1'", value);
            return false;
        }

        return true;
    }

    LogError("Unknown HTTP property '%1'", key);
    return false;
}

bool http_Config::SetPortOrPath(Span<const char> str)
{
    if (std::all_of(str.begin(), str.end(), IsAsciiDigit)) {
        int new_port;
        if (!ParseInt(str, &new_port))
            return false;

        if (new_port <= 0 || port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", port, UINT16_MAX);
            return false;
        }

        if (sock_type != SocketType::IPv4 && sock_type != SocketType::IPv6 && sock_type != SocketType::Dual) {
            sock_type = SocketType::Dual;
        }
        port = new_port;
    } else {
        sock_type = SocketType::Unix;
        unix_path = NormalizePath(str, &str_alloc).ptr;
    }

    return true;
}

bool http_Config::Validate() const
{
    bool valid = true;

    if (sock_type == SocketType::Unix) {
        struct sockaddr_un addr;

        if (!unix_path) {
            LogError("Unix socket path must be set");
            valid = false;
        } else if (strlen(unix_path) >= sizeof(addr.sun_path)) {
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
        case SocketType::IPv6: { listen_fd = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM); } break;
        case SocketType::Unix: { listen_fd = OpenUnixSocket(config.unix_path, SOCK_STREAM); } break;
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
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func,
                        bool log_socket)
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

#if defined(RG_DEBUG)
    flags |= MHD_USE_DEBUG;
#endif

    // MHD options
    LocalArray<MHD_OptionItem, 16> mhd_options;
    mhd_options.Append({ MHD_OPTION_LISTEN_SOCKET, listen_fd, nullptr });
    if (config.threads > 1) {
        mhd_options.Append({ MHD_OPTION_THREAD_POOL_SIZE, config.threads, nullptr });
    }
    if (config.max_connections) {
        mhd_options.Append({ MHD_OPTION_CONNECTION_LIMIT, config.max_connections, nullptr });
    }
    mhd_options.Append({ MHD_OPTION_CONNECTION_TIMEOUT, (intptr_t)(config.idle_timeout / 1000), nullptr });
    mhd_options.Append({ MHD_OPTION_END, 0, nullptr });
    client_addr_mode = config.client_addr_mode;

#if defined(_WIN32)
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

    if (log_socket) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return daemon;
}

void http_Daemon::Stop()
{
    running = false;

    if (async) {
#if defined(_WIN32)
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
#if !defined(_WIN32)
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

            if (!addr.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(addr, out_address)) [[unlikely]] {
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

            if (!addr.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(addr, out_address)) [[unlikely]] {
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

    if (!daemon->running) [[unlikely]] {
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
        if (url[0] != '/') [[unlikely]] {
            io->AttachError(400);
            return MHD_queue_response(conn, (unsigned int)io->code, io->response);
        }

        if (TestStr(method, "HEAD")) {
            io->request.method = http_RequestMethod::Get;
            io->request.headers_only = true;
        } else if (!OptionToEnumI(http_RequestMethodNames, method, &io->request.method)) {
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
                MemCpy(io->read_buf.ptr + io->read_len, upload_data, copy_len);

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
        MemCpy(buf, io->write_buf.ptr + io->write_offset, copy_len);

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

            if (running) [[likely]] {
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

void http_Daemon::RequestCompleted(void *, MHD_Connection *, void **con_cls, MHD_RequestTerminationCode)
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

static void ListConnectionValues(MHD_Connection *conn, MHD_ValueKind kind,
                                 Allocator *alloc, HeapArray<http_KeyValue> *out_pairs)
{
    struct ListContext {
        Allocator *alloc;
        HeapArray<http_KeyValue> *pairs;
    };
    ListContext ctx;
    ctx.alloc = alloc;
    ctx.pairs = out_pairs;

    MHD_get_connection_values(conn, kind, [](void *udata, enum MHD_ValueKind, const char *key, const char *value) {
        ListContext *ctx = (ListContext *)udata;

        http_KeyValue pair;
        pair.key = DuplicateString(key, ctx->alloc).ptr;
        pair.value = DuplicateString(value, ctx->alloc).ptr;

        ctx->pairs->Append(pair);

        return MHD_YES;
    }, &ctx);
}

void http_RequestInfo::ListGetValues(Allocator *alloc, HeapArray<http_KeyValue> *out_pairs) const
{
    ListConnectionValues(conn, MHD_GET_ARGUMENT_KIND, alloc, out_pairs);
}

void http_RequestInfo::ListHeaderValues(Allocator *alloc, HeapArray<http_KeyValue> *out_pairs) const
{
    ListConnectionValues(conn, MHD_HEADER_KIND, alloc, out_pairs);
}

http_IO::http_IO()
    : response(MHD_create_response_empty((MHD_ResponseFlags)0))
{
}

http_IO::~http_IO()
{
    for (const auto &func: finalizers) {
        func();
    }

#if defined(_WIN32)
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
        case CompressionType::LZ4: { RG_UNREACHABLE(); } break;
        case CompressionType::Zstd: { AddHeader("Content-Encoding", "zstd"); } break;
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

void http_IO::AddCachingHeaders(int64_t max_age, const char *etag)
{
    RG_ASSERT(max_age >= 0);

#if defined(RG_DEBUG)
    max_age = 0;
#endif

    if (max_age || etag) {
        char buf[128];

        AddHeader("Cache-Control", max_age ? Fmt(buf, "max-age=%1", max_age / 1000).ptr : "no-store");
        if (etag) {
            AddHeader("ETag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

void http_IO::AttachText(int code, Span<const char> str, const char *mimetype)
{
    MHD_Response *response =
        MHD_create_response_from_buffer(str.len, (void *)str.ptr, MHD_RESPMEM_PERSISTENT);

    AttachResponse(code, response);
    AddHeader("Content-Type", mimetype);
}

void http_IO::AttachBinary(int code, Span<const uint8_t> data, const char *mimetype)
{
    MHD_Response *response =
        MHD_create_response_from_buffer(data.len, (void *)data.ptr, MHD_RESPMEM_PERSISTENT);

    AttachResponse(code, response);

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }
}

bool http_IO::AttachAsset(int code, Span<const uint8_t> data, const char *mimetype,
                          CompressionType src_encoding)
{
    CompressionType dest_encoding;
    if (!NegociateEncoding(src_encoding, &dest_encoding))
        return false;

    if (dest_encoding != src_encoding) {
        if (request.headers_only) {
            AttachEmpty(code);
            AddEncodingHeader(dest_encoding);
        } else {
            if (data.len > Mebibytes(16)) {
                static const char *msg = "Refusing excessive content-encoding conversion size";

                LogError(msg);
                AttachError(415, msg);

                return false;
            }

            RunAsync([=, this]() {
                StreamReader reader(data, nullptr, src_encoding);

                StreamWriter writer;
                if (!OpenForWrite(code, -1, dest_encoding, &writer))
                    return;
                AddEncodingHeader(dest_encoding);

                if (!SpliceStream(&reader, -1, &writer))
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

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
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

bool http_IO::AttachFile(int code, const char *filename, const char *mimetype)
{
    FileInfo file_info;
    if (StatFile(filename, &file_info) != StatResult::Success)
        return false;

    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return false;

    MHD_Response *response = MHD_create_response_from_fd((uint64_t)file_info.size, fd);
    AttachResponse(code, response);

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    return true;
}

void http_IO::AttachEmpty(int code)
{
    MHD_Response *response = MHD_create_response_empty((MHD_ResponseFlags)0);
    AttachResponse(code, response);
}

bool http_IO::OpenForRead(Size max_len, StreamReader *out_st)
{
    RG_ASSERT(state != State::Sync && state != State::WebSocket);

    // Only allow Gzip for now, to reduce attack surface
    CompressionType compression_type = CompressionType::None;
    {
        const char *content_str = request.GetHeaderValue("Content-Encoding");

        if (content_str) {
            if (max_len < 0) {
                LogError("Refusing Content-Encoding without server limit");
                AttachError(400);
                return false;
            }

            if (TestStr(content_str, "gzip")) {
                compression_type = CompressionType::Gzip;
            } else {
                LogError("Refusing Content-Encoding value other than gzip");
                AttachError(400);
                return false;
            }
        }
    }

    // Precheck with Content-Length for quick dismissal, but even
    // if the header is missing the StreamReader will enforce the limit.
    if (max_len >= 0) {
        if (const char *str = request.GetHeaderValue("Content-Length"); str) {
            Size len;
            if (!ParseInt(str, &len)) [[unlikely]] {
                AttachError(400);
                return false;
            }
            if (len < 0) [[unlikely]] {
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

    bool success = out_st->Open([this](Span<uint8_t> out_buf) { return Read(out_buf); }, "<http>", compression_type);
    RG_ASSERT(success);

    out_st->SetReadLimit(max_len);

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

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    finalizers.Append(func);
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

void http_IO::PushLogFilter()
{
    // This log filter does two things: it keeps a copy of the last log error message,
    // and it sets the log context to the client address (for log file).
    RG::PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        if (level == LogLevel::Error) {
            last_err = DuplicateString(msg, &allocator).ptr;
        }

        char ctx_buf[512];
        Fmt(ctx_buf, "%1%2: ", ctx ? ctx : "", request.client_addr);

        func(level, ctx_buf, msg);
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
