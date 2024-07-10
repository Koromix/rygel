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

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>

namespace RG {

class http_Daemon::RequestHandler {
    http_Daemon *daemon;

    int event_fd = -1;
    std::shared_mutex wake_mutex;
    bool wake_needed = false;

    LocalArray<http_IO *, 64> pool;

public:
    RequestHandler(http_Daemon *daemon) : daemon(daemon) {}

    bool Run();
    void Wake();

private:
    http_IO *CreateClient(int fd, struct sockaddr *sa);
    void CloseClient(http_IO *client);
};

static const int KeepAliveCount = 1000;
static const int KeepAliveDelay = 20000;

static RG_CONSTINIT ConstMap<128, int, const char *> ErrorMessages = {
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "Switch Proxy" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Content Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Content" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 449, "Reply With" },
    { 450, "Blocked by Windows Parental Controls" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 506, "Variant Also Negotiates" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 509, "Bandwidth Limit Exceeded" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" }
};

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
    } else if (key == "ClientAddress") {
        if (!OptionToEnumI(http_ClientAddressModeNames, value, &addr_mode)) {
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

    return valid;
}

bool http_Daemon::Bind(const http_Config &config)
{
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

    // Use non-blocking accept
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    addr_mode = config.addr_mode;

    return true;
}

bool http_Daemon::Start(const http_Config &config,
                        std::function<void(const http_RequestInfo &request, http_IO *io)> func,
                        bool log_socket)
{
    RG_ASSERT(!async);
    RG_ASSERT(func);

    // Validate configuration
    if (!config.Validate())
        return false;

    if (config.addr_mode == http_ClientAddressMode::Socket) {
        LogInfo("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-IP "
                "if you run this behind a reverse proxy that sets one of these headers.");
    }

    // Prepare socket (if not done yet)
    if (listen_fd < 0 && !Bind(config))
        return false;

    int fronts = GetCoreCount();
    int backs = GetCoreCount() * 4;
    async = new Async(fronts + backs);

    for (Size i = 0; i < fronts; i++) {
        RequestHandler *handler = new RequestHandler(this);
        handlers.Append(handler);
    }

    handle_func = func;

    // Run request handlers
    for (RequestHandler *handler: handlers) {
        async->Run([=] { return handler->Run(); });
    }

    if (log_socket) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return true;
}

void http_Daemon::Stop()
{
    // Wake up everyone
    shutdown(listen_fd, SHUT_RD);

    // Wait for everyone to wind down
    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    for (RequestHandler *handler: handlers) {
        delete handler;
    }
    handlers.Clear();

    CloseSocket(listen_fd);
    listen_fd = -1;
}

const char *http_RequestInfo::FindHeader(const char *key) const
{
    for (const http_KeyValue &header: headers) {
        if (TestStrI(header.key, key))
            return header.value;
    }

    return nullptr;
}

const char *http_RequestInfo::FindGetValue(const char *) const
{
    LogDebug("Not implemented");
    return nullptr;
}

const char *http_RequestInfo::FindCookie(const char *) const
{
    LogDebug("Not implemented");
    return nullptr;
}

void http_IO::AddHeader(Span<const char> key, Span<const char> value)
{
    http_KeyValue header = {};

    header.key = DuplicateString(key, &allocator).ptr;
    header.value = DuplicateString(value, &allocator).ptr;

    response.headers.Append(header);
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

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value, bool http_only)
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

bool http_IO::NegociateEncoding(CompressionType preferred, CompressionType *out_encoding)
{
    const char *accept_str = request.FindHeader("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    if (acceptable_encodings & (1 << (int)preferred)) {
        *out_encoding = preferred;
        return true;
    } else if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    } else {
        SendError(406);
        return false;
    }
}

bool http_IO::NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding)
{
    const char *accept_str = request.FindHeader("Accept-Encoding");
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
        SendError(406);
        return false;
    }
}

void http_IO::Send(int status, Size len, FunctionRef<void(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    LocalArray<char, 32768> intro;

    const char *protocol = (version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = ErrorMessages.FindValue(status, "Unknown");

    if (keepalive) {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: keep-alive\r\n"
                                                "Keep-Alive: timeout=%4, max=%5\r\n",
                         protocol, status, details, KeepAliveDelay / 1000, KeepAliveCount).len;
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: close\r\n",
                         protocol, status, details).len;
    }

    for (const http_KeyValue &header: response.headers) {
        intro.len += Fmt(intro.TakeAvailable(), "%1: %2\r\n", header.key, header.value).len;
    }

    if (len >= 0) {
        intro.len += Fmt(intro.TakeAvailable(), "Content-Length: %1\r\n\r\n", len).len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            keepalive = false;
            goto exit;
        }

        writer.Write(intro);

        func(fd, &writer);
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "Transfer-Encoding: chunked\r\n\r\n").len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            keepalive = false;
            goto exit;
        }

        writer.Write(intro);

        const auto chunk = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };
        StreamWriter chunker(chunk, "<http>");

        func(-1, &chunker);
        writer.Write("0\r\n\r\n");
    }

    keepalive &= writer.IsValid();

exit:
    response.sent = true;

    int off = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
}

void http_IO::SendText(int status, Span<const char> text, const char *mimetype)
{
    RG_ASSERT(mimetype);
    AddHeader("Content-Type", mimetype);

    Send(status, text.len, [&](int, StreamWriter *writer) {
        writer->Write(text);
    });
}

void http_IO::SendBinary(int status, Span<const uint8_t> data, const char *mimetype)
{
    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, data.len, [&](int, StreamWriter *writer) {
        writer->Write(data);
    });
}

void http_IO::SendError(int status, const char *details)
{
    const char *last_err = nullptr; // XXX

    if (!details) {
        details = (status < 500 && last_err) ? last_err : "";
    }

    Span<char> text = Fmt(&allocator, "Error %1: %2\n%3", status,
                          ErrorMessages.FindValue(status, "Unknown"), details);
    SendText(status, text);
}

bool http_IO::SendFile(int status, const char *filename, const char *mimetype)
{
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return false;
    RG_DEFER { CloseDescriptor(fd); };

    struct stat sb;
    if (fstat(fd, &sb) < 0)
        return false;
    if (!S_ISREG(sb.st_mode)) {
        LogError("Not a regular file: %1", filename);
        return false;
    }
    int64_t len = (int64_t)sb.st_size;

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, len, [&](int sock, StreamWriter *) {
        off_t offset = 0;
        int64_t remain = len;

        while (remain) {
            Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);
            Size sent = sendfile(sock, fd, &offset, (size_t)send);

            if (sent < 0) {
                if (errno != EPIPE) {
                    LogError("Failed to send file: %1", strerror(errno));
                }
                return;
            }

            remain -= sent;
        }
    });

    return true;
}

void http_IO::SendEmpty(int status)
{
    Send(status, 0, [](int, StreamWriter *) {});
}

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    RG_ASSERT(!response.sent);
    response.finalizers.Append(func);
}

bool http_Daemon::RequestHandler::Run()
{
    RG_ASSERT(event_fd < 0);

    int listen_fd = daemon->listen_fd;
    http_ClientAddressMode addr_mode = daemon->addr_mode;

    Async async(daemon->async);
    BucketArray<http_IO *, 2048> clients;

    event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (event_fd < 0) {
        LogError("Failed to create eventfd: %1", strerror(errno));
        return false;
    }
    RG_DEFER {
        CloseDescriptor(event_fd);
        event_fd = -1;
    };

    // Delete remaining clients when function exits
    RG_DEFER {
        async.Sync();

        for (const http_IO *client: clients) {
            delete client;
        };
        for (const http_IO *client: pool) {
            delete client;
        }
        pool.Clear();
    };

    HeapArray<struct pollfd> pfds = {
        { listen_fd, POLLIN, 0 },
        { event_fd, POLLIN, 0 }
    };

    int64_t busy = 0;

    for (;;) {
        int64_t now = GetMonotonicTime();

        pfds.RemoveFrom(2);

        if (pfds[0].revents & POLLHUP)
            return true;
        if (pfds[0].revents & POLLIN) {
            sockaddr_storage ss;
            socklen_t ss_len = RG_SIZE(ss);

            // Accept queued clients
            for (Size i = 0; i < 64; i++) {
                int fd = accept4(listen_fd, (sockaddr *)&ss, &ss_len, SOCK_CLOEXEC);

                if (fd < 0) {
                    if (errno == EINVAL)
                        return true;

                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        pfds[0].revents &= ~POLLIN;
                        break;
                    }

                    LogError("Failed to accept client: %1", strerror(errno));
                    return false;
                }

                http_IO *client = CreateClient(fd, (sockaddr *)&ss);
                clients.Append(client);

                busy = now;
            }
        }

        // Clear eventfd
        if (pfds[1].revents & POLLIN) {
            uint64_t dummy = 0;
            ssize_t ret = read(event_fd, &dummy, RG_SIZE(dummy));
            (void)ret;
        }

        auto begin = clients.begin();
        Size removed = 0;

        const auto forget = [&](auto it) {
            std::swap(*begin, *it);
            begin++;
            removed++;
        };

        for (auto it = begin; it != clients.end(); it++) {
            http_IO *client = *it;
            http_IO::PrepareStatus ret = client->Prepare();

            switch (ret) {
                case http_IO::PrepareStatus::Waiting: {
                    struct pollfd pfd = { client->Descriptor(), POLLIN, 0 };
                    pfds.Append(pfd);
                } break;

                case http_IO::PrepareStatus::Ready: {
                    if (!client->InitAddress(addr_mode)) {
                        client->keepalive = false;
                        client->SendText(400, "Malformed request");

                        forget(it);
                        CloseClient(client);

                        break;
                    }

                    client->keepalive &= (--client->count <= 0) || (now <= client->timeout);

                    if (client->keepalive) {
                        async.Run([=, this] {
                            daemon->handle_func(client->request, client);

                            client->Reset();
                            Wake();

                            return true;
                        });
                    } else {
                        async.Run([=, this] {
                            daemon->handle_func(client->request, client);
                            client->Close();

                            return true;
                        });
                    }
                } break;

                case http_IO::PrepareStatus::Busy: {} break;

                case http_IO::PrepareStatus::Closed: {
                    forget(it);
                    CloseClient(client);
                } break;

                case http_IO::PrepareStatus::Error: {
                    client->keepalive = false;
                    client->SendText(400, "Malformed request");

                    forget(it);
                    CloseClient(client);
                } break;
            }
        }

        clients.RemoveFirst(removed);

        if (now - busy > 1000) {
            // Wake me up from the kernel if needed
            {
                std::lock_guard<std::shared_mutex> lock_excl(wake_mutex);
                wake_needed = true;
            }

            if (poll(pfds.ptr, pfds.len, -1) < 0) {
                LogError("Failed to poll descriptors: %1", strerror(errno));
                return false;
            }

            busy = GetMonotonicTime();
        }
    }

    RG_UNREACHABLE();
}

void http_Daemon::RequestHandler::Wake()
{
    std::shared_lock<std::shared_mutex> lock_shr(wake_mutex);

    if (wake_needed) {
        lock_shr.unlock();

        uint64_t one = 1;
        ssize_t ret = write(event_fd, &one, RG_SIZE(one));
        (void)ret;

        wake_needed = false;
    }
}

http_IO *http_Daemon::RequestHandler::CreateClient(int fd, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (pool.len) {
        int idx = GetRandomInt(0, pool.len);

        client = pool[idx];

        std::swap(pool[idx], pool[pool.len - 1]);
        pool.len--;
    } else {
        client = new http_IO(this);
    }

    if (!client->Init(fd, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Daemon::RequestHandler::CloseClient(http_IO *client)
{
    if (pool.Available()) {
        pool.Append(client);
        client->Reset();
    } else {
        delete client;
    }
}

http_IO::http_IO(RequestHandler *handler)
    : handler(handler)
{
    Reset();

    count = KeepAliveCount;
    timeout = GetMonotonicTime() + KeepAliveDelay;
}

http_IO::PrepareStatus http_IO::Prepare()
{
    if (ready)
        return PrepareStatus::Busy;
    if (fd < 0)
        return PrepareStatus::Closed;

    // Gather request line and headers
    {
        Size pos = std::max(buf.len - 3, (Size)0);
        bool complete = false;

        buf.Grow(Mebibytes(2));

        while (!complete) {
            Size read = recv(fd, buf.end(), buf.Available(), MSG_DONTWAIT);

            if (read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return PrepareStatus::Waiting;
                if (errno == ECONNRESET)
                    return PrepareStatus::Error;

                LogError("Read failed: %1 %2", strerror(errno), errno);
                return PrepareStatus::Error;
            }

            if (!read) {
                if (!buf.len) {
                    Close();
                    return PrepareStatus::Closed;
                }

                LogError("Malformed HTTP request");
                return PrepareStatus::Error;
            }

            buf.len += read;

            for (;;) {
                uint8_t *next = (uint8_t *)memchr(buf.ptr + pos, '\r', buf.len - pos);
                pos = next ? next - buf.ptr : buf.len;

                if (buf.len - pos < 4)
                    break;

                if (buf[pos] == '\r' && buf[pos + 1] == '\n' &&
                        buf[pos + 2] == '\r' && buf[pos + 3] == '\n') {
                    pos += 4;
                    complete = true;

                    break;
                }

                pos++;
            }
        }

        extra = MakeSpan(buf.ptr + pos, buf.len - pos);

        buf.len = pos - 4;
        buf.ptr[buf.len] = 0;
        intro = buf.As<char>();
    }

    Span<char> remain = intro;

    // Parse request line
    {
        Span<char> line = SplitStrLine(remain, &remain);

        Span<char> method = SplitStr(line, ' ', &line);
        Span<char> url = SplitStr(line, ' ', &line);
        Span<char> protocol = SplitStr(line, ' ', &line);

        for (char &c: method) {
            c = UpperAscii(c);
        }

        if (!method.len) {
            SendError(400, "Empty HTTP method");
            return PrepareStatus::Error;
        }
        if (!StartsWith(url, "/")) {
            SendError(400, "Invalid request URL");
            return PrepareStatus::Error;
        }
        if (TestStrI(protocol, "HTTP/1.0")) {
            version = 10;
            keepalive = false;
        } else if (TestStrI(protocol, "HTTP/1.1")) {
            version = 11;
            keepalive = true;
        } else {
            SendError(400, "Invalid HTTP version");
            return PrepareStatus::Error;
        }
        if (line.len) {
            SendError(400, "Unexpected data after request line");
            return PrepareStatus::Error;
        }

        if (TestStr(method, "HEAD")) {
            request.method = http_RequestMethod::Get;
            request.headers_only = true;
        } else if (!OptionToEnumI(http_RequestMethodNames, method, &request.method)) {
            SendError(405);
            return PrepareStatus::Error;
        }

        Span<char> query;
        url = SplitStr(url, '?', &query);

        url.ptr[url.len] = 0;
        request.url = url.ptr;
    }

    // Parse headers
    while (remain.len) {
        http_KeyValue header = {};

        Span<char> line = SplitStrLine(remain, &remain);

        Span<char> key = TrimStrRight(SplitStr(line, ':', &line));
        if (line.ptr == key.end()) {
            LogError("Malformed header line");
            return PrepareStatus::Error;
        }
        Span<char> value = TrimStr(line);

        bool upper = true;
        for (char &c: key.As<char>()) {
            c = upper ? UpperAscii(c) : LowerAscii(c);
            upper = (c == '-');
        }

        key.ptr[key.len] = 0;
        value.ptr[value.len] = 0;

        header.key = key.ptr;
        header.value = value.ptr;

        request.headers.Append(header);

        if (TestStrI(key, "Connection")) {
            keepalive = !TestStrI(value, "close");
        }
    }

    ready = true;
    return PrepareStatus::Ready;
}

bool http_IO::Init(int fd, struct sockaddr *sa)
{
    this->fd = fd;

    int family = sa->sa_family;
    void *ptr = nullptr;

    switch (family) {
        case AF_INET: { ptr = &((sockaddr_in *)sa)->sin_addr; } break;
        case AF_INET6: { ptr = &((sockaddr_in6 *)sa)->sin6_addr; } break;
        case AF_UNIX: {
            CopyString("unix", addr);
            return true;
        } break;

        default: { RG_UNREACHABLE(); } break;
    }

    if (!inet_ntop(family, ptr, addr, RG_SIZE(addr))) {
        LogError("Cannot convert network address to text");
        return false;
    }

    return true;
}

bool http_IO::InitAddress(http_ClientAddressMode addr_mode)
{
    switch (addr_mode) {
        case http_ClientAddressMode::Socket: { /* Keep socket address */ } break;

        case http_ClientAddressMode::XForwardedFor: {
            const char *str = request.FindHeader("X-Forwarded-For");
            if (!str) {
                LogError("X-Forwarded-For header is missing but is required by the configuration");
                return false;
            }

            Span<const char> trimmed = TrimStr(SplitStr(str, ','));

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }
        } break;

        case http_ClientAddressMode::XRealIP: {
            const char *str = request.FindHeader("X-Real-IP");
            if (!str) {
                LogError("X-Real-IP header is missing but is required by the configuration");
                return false;
            }

            Span<const char> trimmed = TrimStr(str);

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                return false;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                return false;
            }
        } break;
    }

    return true;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
        Size sent = send(fd, data.ptr, (size_t)data.len, MSG_MORE | MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno != EPIPE) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        data.ptr += sent;
        data.len -= sent;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    while (data.len) {
        LocalArray<uint8_t, 16384> buf;

        Size copy_len = std::min(RG_SIZE(buf.data) - 8, data.len);

        buf.len = 8 + copy_len;
        Fmt(buf.As<char>(), "%1\r\n", FmtHex(copy_len).Pad0(-4));
        MemCpy(buf.data + 6, data.ptr, copy_len);
        buf.data[6 + copy_len + 0] = '\r';
        buf.data[6 + copy_len + 1] = '\n';

        Span<const uint8_t> remain = buf;

        do {
            Size sent = send(fd, remain.ptr, (size_t)remain.len, MSG_MORE | MSG_NOSIGNAL);

            if (sent < 0) {
                if (errno != EPIPE) {
                    LogError("Failed to send to client: %1", strerror(errno));
                }
                return false;
            }

            remain.ptr += sent;
            remain.len -= sent;
         } while (remain.len);

        data.ptr += copy_len;
        data.len -= copy_len;
    }

    return true;
}

void http_IO::Reset()
{
    for (const auto &finalize: response.finalizers) {
        finalize();
    }

    buf.RemoveFrom(0);
    allocator.Reset();

    intro = {};
    extra = {};

    version = 10;
    keepalive = false;
    request = {};

    response = {};

    ready = false;
}

void http_IO::Close()
{
    for (const auto &finalize: response.finalizers) {
        finalize();
    }

    CloseSocket(fd);
    fd = -1;

    ready = false;
}

}
