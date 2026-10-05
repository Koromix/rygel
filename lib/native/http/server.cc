// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "server.hh"
#include "misc.hh"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <ws2tcpip.h>

    #if !defined(UNIX_PATH_MAX)
        #define UNIX_PATH_MAX 108
    #endif
    typedef struct sockaddr_un {
        ADDRESS_FAMILY sun_family;
        char sun_path[UNIX_PATH_MAX];
    } SOCKADDR_UN, *PSOCKADDR_UN;
#else
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
#endif

namespace K {

const HashMap<int, const char *> http_ErrorMessages = {
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
    } else if (key == "BindIP") {
        if (value == "*") {
            bind_addr = nullptr;
        } else {
            bind_addr = DuplicateString(value, &str_alloc).ptr;
        }

        return true;
    } else if (key == "Port") {
        return ParseInt(value, &port);
    } else if (key == "UnixPath") {
        unix_path = NormalizePath(value, root_directory, &str_alloc).ptr;
        return true;
    } else if (key == "ClientAddress") {
        if (!OptionToEnumI(http_AddressModeNames, value, &addr_mode)) {
            LogError("Unknown client address mode '%1'", value);
            return false;
        }

        return true;
    } else if (key == "IdleTimeout") {
        return ParseDuration(value, &idle_timeout);
    } else if (key == "KeepAliveTime") {
        if (value == "Disabled") {
            keepalive_time = 0;
            return true;
        } else {
            return ParseDuration(value, &keepalive_time);
        }
    } else if (key == "SendTimeout") {
        return ParseDuration(value, &send_timeout);
    } else if (key == "LingerTimeout") {
        return ParseDuration(value, &linger_timeout);
    } else if (key == "StopTimeout") {
        return ParseDuration(value, &stop_timeout);
    } else if (key == "MaxRequestSize") {
        return ParseSize(value, &max_request_size);
    } else if (key == "MaxUrlLength") {
        return ParseSize(value, &max_url_len);
    } else if (key == "MaxRequestHeaders") {
        return ParseInt(value, &max_request_headers);
    } else if (key == "MaxRequestCookies") {
        return ParseInt(value, &max_request_cookies);
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

        if (new_port <= 0 || new_port > UINT16_MAX) {
            LogError("HTTP port %1 is invalid (range: 1 - %2)", new_port, UINT16_MAX);
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

    if (idle_timeout < 1000) {
        LogError("HTTP IdleTimeout must be >= 1 sec");
        valid = false;
    }
    if (keepalive_time && keepalive_time < 5000) {
        LogError("HTTP KeepAliveTime must be >= 5 sec (or Disabled)");
        valid = false;
    }
    if (send_timeout < 10000) {
        LogError("HTTP SendTimeout must be >= 10 sec");
        valid = false;
    }
    if (stop_timeout < 1000) {
        LogError("HTTP StopTimeout must be >= 1 sec");
        valid = false;
    }

    if (max_request_size < 1024) {
        LogError("MaxRequestSize must be >= 1 kB");
        valid = false;
    }
    if (max_url_len < 512) {
        LogError("MaxUrlLength must be >= 512 B");
        valid = false;
    }
    if (max_request_headers < 16) {
        LogError("MaxRequestHeaders must be >= 16");
        valid = false;
    }
    if (max_request_cookies < 0) {
        LogError("MaxRequestCookies must be >= 0");
        valid = false;
    }

    return valid;
}

static void SetPortReuse(int sock, bool enable)
{
#if defined(SO_REUSEPORT_LB)
    int reuse = enable;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &reuse, sizeof(reuse));
#elif defined(SO_REUSEPORT)
    int reuse = enable;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#else
    (void)sock;
    (void)enable;
#endif
}

static int CreateListenSocket(const http_Config &config, bool first)
{
    int sock = CreateSocket(config.sock_type, SOCK_STREAM);
    if (sock < 0)
        return -1;
    K_DEFER_N(err_guard) { CloseSocket(sock); };

    // Set SO_REUSEPORT after first connection, so that two HTTP serving processes
    // don't end up overlapping each other.
    SetPortReuse(sock, !first);

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: {
            if (!BindIPSocket(sock, config.sock_type, config.bind_addr, config.port))
                return -1;
        } break;
        case SocketType::Unix: {
            if (!BindUnixSocket(sock, config.unix_path))
                return -1;
        } break;
    }

    if (first) {
        // The bind succeeded, we know that no other process is using this port.
        // Let the next sockets reuse this port.
        SetPortReuse(sock, true);
    }

    if (listen(sock, 200) < 0) {
#if defined(_WIN32)
        LogError("Failed to listen on socket: %1", GetWin32ErrorString());
        return -1;
#else
        LogError("Failed to listen on socket: %1", strerror(errno));
        return -1;
#endif
    }

    SetSocketNonBlock(sock, true);

    err_guard.Disable();
    return sock;
}

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    K_ASSERT(!listeners.len);
    K_ASSERT(!dispatchers.len);

    if (!config.Validate())
        return false;

    if (config.addr_mode == http_AddressMode::Socket) {
        LogWarning("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-Ip "
                   "if you run this behind a reverse proxy that sets one of these headers.");
    }

    // Copy main confg values
    sock_type = config.sock_type;
    addr_mode = config.addr_mode;
    idle_timeout = config.idle_timeout;
    keepalive_time = config.keepalive_time;
    send_timeout = config.send_timeout;
    linger_timeout = config.linger_timeout;
    stop_timeout = config.stop_timeout;
    max_request_size = config.max_request_size;
    max_url_len = config.max_url_len;
    max_request_headers = config.max_request_headers;
    max_request_cookies = config.max_request_cookies;

#if defined(_WIN32)
    if (!InitWinsock())
        return false;
#endif

    K_DEFER_N(err_guard) {
        for (int listener: listeners) {
            CloseSocket(listener);
        }
        listeners.Clear();
    };

#if defined(_WIN32)
    // It is not possible to bind multiple sockets to a single TCP port on Windows
    // safely, because SO_EXCLUSIVEADDR must be set before bind(), so it cannot be used
    // and other processes can hijack us at any time (instead of during a small window).
    // In addition, running accept() from multiple threads on the same listening socket
    // will return the same socket to multiple threads which breaks everything.
    // Yes, this happens. In 2026.
    dispatchers.AppendDefault(1);
#else
    dispatchers.AppendDefault(GetCoreCount());
#endif

    for (Size i = 0; i < dispatchers.len; i++) {
        int listener = CreateListenSocket(config, !i);
        if (listener < 0)
            return false;
        listeners.Append(listener);

        // One cannot bind to the same UNIX socket multiple times
        if (config.sock_type == SocketType::Unix)
            break;
    }

    if (log_addr) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    err_guard.Disable();
    return true;
}

bool http_Daemon::Start(std::function<void(http_IO *io)> func)
{
    K_ASSERT(listeners.len);
    K_ASSERT(dispatchers.len);
    K_ASSERT(!handle_func);
    K_ASSERT(func);

    handle_func = func;

    int handlers = std::max(16, 4 * GetCoreCount());
    async = new Async(handlers, (int)AsyncFlag::Background);

    for (Size i = 0; i < dispatchers.len; i++) {
        K_ASSERT(!dispatchers[i].dispatcher);

        int listener = listeners[i % listeners.len];
        http_Dispatcher *dispatcher = CreateDispatcher(listener);

        if (!dispatcher)
            return false;

        dispatchers[i].dispatcher = dispatcher;
    }

    // All engines are running
    for (DispatcherThread &it: dispatchers) {
        it.thread = std::thread(&http_Daemon::RunDispatcher, it.dispatcher);
    }

    return true;
}

void http_Daemon::Stop()
{
    // Shut everything down
    // On Windows and macOS (and maybe others), the shutdown() does not wake up poll() so use
    // the pipe to wake it up and signal the ongoing shutdown.
    for (int listener: listeners) {
#if defined(_WIN32)
        shutdown(listener, SD_BOTH);
#else
        shutdown(listener, SHUT_RDWR);
#endif
    }

    for (DispatcherThread &it: dispatchers) {
        if (it.dispatcher) {
            StopDispatcher(it.dispatcher);
        }
        if (it.thread.joinable()) {
            it.thread.join();
        }
        DestroyDispatcher(it.dispatcher);
    }
    dispatchers.Clear();

    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    for (int listener: listeners) {
        CloseSocket(listener);
    }
    listeners.Clear();

    handle_func = {};
}

void http_Daemon::RunHandler(http_IO *client)
{
    // This log filter does two things: it keeps a copy of the last log error message,
    // and it sets the log context to the client address (for log file).
    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        if (level == LogLevel::Error) {
            client->last_err = DuplicateString(msg, &client->allocator).ptr;
        }

        char ctx_buf[512];
        Fmt(ctx_buf, "%1%2: ", ctx ? ctx : "", client->request.client_addr);

        func(level, ctx_buf, msg);
    });
    K_DEFER { PopLogFilter(); };

    int64_t now = GetMonotonicClock();
    client->request.keepalive &= (now < client->socket_start + keepalive_time);

    handle_func(client);

    if (!client->response.started) [[unlikely]] {
        client->SendError(500);
    }
}

static inline bool IsFieldKeyValid(Span<const char> key)
{
    static K_CONSTINIT Bitset<256> ValidCharacters = {
        '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '0', '1', '2', '3', '4', '5',
        '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
        'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '^', '_',
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '|', '~'
    };

    if (!key.len)
        return false;

    bool valid = std::all_of(key.begin(), key.end(), [](char c) { return ValidCharacters.Test((uint8_t)c); });
    return valid;
}

static inline bool IsFieldValueValid(Span<const char> key)
{
    static K_CONSTINIT Bitset<256> ValidCharacters = {
        '\t', ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.',
        '/', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>',
        '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
        '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~'
    };

    bool valid = std::all_of(key.begin(), key.end(), [](char c) { return ValidCharacters.Test((uint8_t)c); });
    return valid;
}

[[maybe_unused]] static bool IsHeaderKeyValid(Span<const char> key)
{
    bool upper = true;

    for (int c: key) {
        bool valid = upper ? (c == UpperAscii(c)) : (c == LowerAscii(c));
        if (!valid)
            return false;
        upper = (c == '-');
    }

    return key.len;
}

const http_KeyHead *http_RequestInfo::FindQuery(const char *key) const
{
    const http_KeyHead *head = values_map.Find(key);
    return head;
}

const http_KeyHead *http_RequestInfo::FindHeader(const char *key) const
{
    K_ASSERT(IsHeaderKeyValid(key));

    const http_KeyHead *head = headers_map.Find(key);
    return head;
}

const http_KeyHead *http_RequestInfo::FindCookie(const char *key) const
{
    const http_KeyHead *head = cookies_map.Find(key);
    return head;
}

const char *http_RequestInfo::GetQueryValue(const char *key) const
{
    const http_KeyHead *head = values_map.Find(key);
    return head ? head->last->value : nullptr;
}

const char *http_RequestInfo::GetHeaderValue(const char *key) const
{
    K_ASSERT(IsHeaderKeyValid(key));

    const http_KeyHead *head = headers_map.Find(key);
    return head ? head->last->value : nullptr;
}

const char *http_RequestInfo::GetCookieValue(const char *key) const
{
    const http_KeyHead *head = cookies_map.Find(key);
    return head ? head->last->value : nullptr;
}

class FmtHeaderValue {
    const char *str;

public:
    FmtHeaderValue(const char *str) : str(str) {}

    void Format(FunctionRef<void(Span<const char>)> append) const;
    operator FmtArg() const { return FmtCustom(*this); }
};

class FmtCookieValue {
    const char *str;

public:
    FmtCookieValue(const char *str) : str(str) {}

    void Format(FunctionRef<void(Span<const char>)> append) const;
    operator FmtArg() const { return FmtCustom(*this); }
};

void FmtHeaderValue::Format(FunctionRef<void(Span<const char>)> append) const
{
    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (IsAsciiControl(c) && c != '\t') {
            append(' ');
        } else {
            append((char)c);
        }
    }
}

void FmtCookieValue::Format(FunctionRef<void(Span<const char>)> append) const
{
    const char *quotes = strpbrk(str, " ,") ? "\"" : "";

    append(quotes);

    for (Size i = 0; str[i]; i++) {
        int c = str[i];

        if (IsAsciiControl(c) || strchr("\";\\", c)) {
            append(' ');
        } else {
            append((char)c);
        }
    }

    append(quotes);
}

bool http_IO::OpenForRead(int64_t max_len, StreamReader *out_st)
{
    K_ASSERT(socket);
    K_ASSERT(!incoming.reading);

    if (max_len >= 0 && request.body_len > max_len) {
        LogError("HTTP body is too big (max = %1)", FmtDiskSize(max_len));
        SendError(413);
        return false;
    }

    daemon->StartRead(socket);

    incoming.reading = true;
    SetTimeout(GetMonotonicClock() + daemon->idle_timeout);

    bool success = out_st->Open([this](Span<uint8_t> out_buf) { return ReadDirect(out_buf); }, "<http>");
    K_ASSERT(success);

    // Additional precaution
    out_st->SetReadLimit(max_len);

    return true;
}

void http_IO::AddHeader(Span<const char> key, Span<const char> value, bool force)
{
    K_ASSERT(!response.started);
    K_ASSERT(IsHeaderKeyValid(key) || force);

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
        case CompressionType::Zstd: { AddHeader("Content-Encoding", "zstd"); } break;
    }
}

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value, unsigned int flags, int max_age)
{
    K_ASSERT(!response.started);

    // Delete if value is NULL
    max_age = value ? max_age : 0;
    value = value ? value : "";

    HeapArray<char> buf(&allocator);

    Fmt(&buf, "%1=%2; Path=%3;", name, FmtCookieValue(value), path);
    if (max_age >= 0) {
        Fmt(&buf, " Max-Age=%1;", max_age / 1000);
    }
    if (flags & (int)http_CookieFlag::SameSiteStrict) {
        Fmt(&buf, " SameSite=Strict;");
    } else {
        Fmt(&buf, " SameSite=Lax;");
    }
    if (flags & (int)http_CookieFlag::HttpOnly) {
        Fmt(&buf, " HttpOnly;");
    }
    if (flags & (int)http_CookieFlag::Secure) {
        Fmt(&buf, " Secure;");
    }

    const char *header = buf.TrimAndLeak(1).ptr;
    response.headers.Append({ "Set-Cookie", header, nullptr });
}

void http_IO::AddCachingHeaders(int64_t max_age, const char *etag)
{
    K_ASSERT(max_age >= 0);

#if defined(K_DEBUG)
    max_age = 0;
    etag = nullptr;
#endif

    if (max_age) {
        char buf[128];
        AddHeader("Cache-Control", max_age ? Fmt(buf, "max-age=%1", max_age / 1000).ptr : "no-store");

        if (etag) {
            AddHeader("Etag", etag);
        }
    } else {
        AddHeader("Cache-Control", "no-store");
    }
}

bool http_IO::NegociateEncoding(CompressionType *out_encoding)
{
    const char *accept_str = request.GetHeaderValue("Accept-Encoding");
    uint32_t acceptable_encodings = http_ParseAcceptableEncodings(accept_str);

    static_assert((int)CompressionType::Zstd > (int)CompressionType::Brotli);
    static_assert((int)CompressionType::Brotli > (int)CompressionType::Gzip);
    static_assert((int)CompressionType::Gzip > (int)CompressionType::Zlib);
    static_assert((int)CompressionType::Zlib > (int)CompressionType::None);

    if (acceptable_encodings) {
        int clz = 31 - CountLeadingZeros(acceptable_encodings);
        *out_encoding = (CompressionType)clz;

        return true;
    }

    SendError(406);
    return false;
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
    }

    SendError(406);
    return false;
}

bool http_IO::OpenForWrite(int status, CompressionType encoding, int64_t len, StreamWriter *out_st)
{
    if (!StartResponse())
        return false;

    bool chunked = len < 0;

    if (chunked && request.version < 11) {
        request.keepalive = false;
        chunked = false;
    }

    // Send response headers
    {
        Span<const char> intro = PrepareResponse(status, encoding, len, chunked);

        if (!WriteDirect(intro.As<const uint8_t>()))
            return false;
    }

    if (request.head) {
        const auto skip = [this](Span<const uint8_t> buf) {
            if (!buf.len) {
                daemon->EndWrite(socket);
            }
            return true;
        };
        return out_st->Open(skip, "<http>");
    } else if (chunked) {
        const auto write = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };
        return out_st->Open(write, "<http>", 0, encoding, CompressionSpeed::Fast);
    } else {
        const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
        return out_st->Open(write, "<http>", 0, encoding, CompressionSpeed::Fast);
    }
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(StreamWriter *)> func)
{
    StreamWriter writer;
    if (!OpenForWrite(status, encoding, len, &writer)) [[unlikely]]
        return;

    request.keepalive &= request.head || func(&writer);
    request.keepalive &= writer.Close();
}

void http_IO::SendEmpty(int status)
{
    Send(status, 0, [](StreamWriter *) { return true; });
}

void http_IO::SendText(int status, Span<const char> text, const char *mimetype)
{
    K_ASSERT(mimetype);
    AddHeader("Content-Type", mimetype);

    Send(status, text.len, [&](StreamWriter *writer) { return writer->Write(text); });
}

void http_IO::SendBinary(int status, Span<const uint8_t> data, const char *mimetype)
{
    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, data.len, [&](StreamWriter *writer) { return writer->Write(data); });
}

void http_IO::SendAsset(int status, Span<const uint8_t> data, const char *mimetype,
                        CompressionType src_encoding)
{
    CompressionType dest_encoding;
    if (!NegociateEncoding(src_encoding, &dest_encoding))
        return;

    if (dest_encoding != src_encoding) {
        if (data.len > Mebibytes(16)) {
            LogError("Refusing excessive Content-Encoding conversion size");
            SendError(415);
            return;
        }

        if (mimetype) {
            AddHeader("Content-Type", mimetype);
        }

        if (request.head) {
            SendEmpty(status);
        } else {
            StreamReader reader(data, "<asset>", src_encoding);
            Send(status, dest_encoding, -1, [&](StreamWriter *writer) { return SpliceStream(&reader, -1, writer); });
        }
    } else {
        AddEncodingHeader(dest_encoding);
        SendBinary(status, data, mimetype);
    }
}

void http_IO::SendError(int status, const char *msg)
{
    if (!msg) {
        msg = (status < 500 && last_err) ? last_err : "";
    }

    const char *error = http_ErrorMessages.FindValue(status, "Unknown");
    Span<char> text = Fmt(&allocator, "Error %1: %2\n%3", status, error, msg);

    return SendText(status, text);
}

void http_IO::SendFile(int status, const char *filename, const char *mimetype)
{
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return;
    K_DEFER { CloseDescriptor(fd); };

    FileInfo file_info;
    if (StatFile(fd, filename, &file_info) != StatResult::Success)
        return;
    if (file_info.type != FileType::File) {
        LogError("Cannot serve non-regular file '%1'", filename);
        return;
    }

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    SendFile(status, fd, file_info.size);
}

void http_IO::SetTimeout(int64_t timeout)
{
    timeout_at.store(timeout, std::memory_order_relaxed);
}

void http_IO::ExtendTimeout(int64_t extend)
{
    int64_t current = timeout_at.load(std::memory_order_relaxed);
    int64_t next = 0;

    do {
        next = GetMonotonicClock() + extend;
    } while (next > current && !timeout_at.compare_exchange_weak(current, next, std::memory_order_relaxed));
}

bool http_IO::Init(http_Socket *socket, int64_t start, struct sockaddr *sa)
{
    this->socket = socket;

    switch (sa->sa_family) {
        case AF_INET: {
            uint8_t *bytes = (uint8_t *)&((sockaddr_in *)sa)->sin_addr;
            Fmt(addr, "%1.%2.%3.%4", bytes[0], bytes[1], bytes[2], bytes[3]);
        } break;

        case AF_INET6: {
#if !defined(_WIN32)
            K_ASSERT(K_SIZE(addr) >= INET6_ADDRSTRLEN + 2);
#endif

            auto *ptr = &((sockaddr_in6 *)sa)->sin6_addr;

            if (IN6_IS_ADDR_V4MAPPED(ptr)) {
                uint8_t *bytes = (uint8_t *)ptr + 12;
                Fmt(addr, "%1.%2.%3.%4", bytes[0], bytes[1], bytes[2], bytes[3]);
            } else {
                const char *ret = inet_ntop(AF_INET6, ptr, addr, K_SIZE(addr));
                K_ASSERT(ret);
            }
        } break;

        case AF_UNIX: { CopyString("unix", addr); } break;

        default: { K_UNREACHABLE(); } break;
    }

    socket_start = start;
    SetTimeout(GetMonotonicClock() + daemon->idle_timeout);

    return true;
}

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

static Size DecodePath(Span<char> str)
{
    Size j = 0;
    for (Size i = 0; i < str.len; i++, j++) {
        str[j] = str[i];

        if (str[i] == '%') {
            if (i > str.len - 3) [[unlikely]] {
                LogError("Truncated %%-encoded value in URL path");
                return -1;
            }

            int high = ParseHexadecimalChar(str.ptr[++i]);
            int low = ParseHexadecimalChar(str.ptr[++i]);

            if (high < 0 || low < 0) [[unlikely]] {
                LogError("Malformed %%-encoded value in URL path");
                return -1;
            }

            str[j] = (char)((high << 4) | low);
        }

        if (IsAsciiControl(str[j])) [[unlikely]] {
            LogError("Unexpected control character in HTTP request line");
            return -1;
        }
    }
    str.len = j;

    if (!IsValidUtf8(str)) {
        LogError("Invalid UTF-8 in URL path");
        return -1;
    }

    return str.len;
}

static Size DecodeQueryComponent(Span<char> str)
{
    Size j = 0;
    for (Size i = 0; i < str.len; i++, j++) {
        str[j] = str[i];

        if (str[i] == '+') {
            str[j] = ' ';
        } else if (str[i] == '%') {
            if (i > str.len - 3) [[unlikely]] {
                LogError("Truncated %%-encoded value in query string");
                return -1;
            }

            int high = ParseHexadecimalChar(str.ptr[++i]);
            int low = ParseHexadecimalChar(str.ptr[++i]);

            if (high < 0 || low < 0) [[unlikely]] {
                LogError("Malformed %%-encoded value in query string");
                return -1;
            }

            str[j] = (char)((high << 4) | low);
        }

        if (IsAsciiControl(str[j])) [[unlikely]] {
            LogError("Unexpected control character in HTTP request line");
            return -1;
        }
    }
    str.len = j;

    if (!IsValidUtf8(str)) {
        LogError("Invalid UTF-8 in query string");
        return -1;
    }

    return str.len;
}

static bool DecodeQuery(Span<char> str, HeapArray<http_KeyValue> *out_values)
{
    str = SplitStr(str, '#');

    while (str.len) {
        Span<char> frag = SplitStr(str, '&', &str);

        if (frag.len) {
            http_KeyValue pair;

            Span<char> value;
            Span<char> key = SplitStr(frag, '=', &value);

            key.len = DecodeQueryComponent(key);
            value.len = DecodeQueryComponent(value);
            if (key.len < 0 || value.len < 0)
                return false;

            key.ptr[key.len] = 0;
            value.ptr[value.len] = 0;
            pair.key = key.ptr;
            pair.value = value.ptr;

            out_values->Append(pair);
        }
    }

    return true;
}

static void MapKeys(Span<http_KeyValue> pairs, HashTable<const char *, http_KeyHead> *out_map)
{
    for (http_KeyValue &pair: pairs) {
        http_KeyHead *head = out_map->InsertOrGet({ pair.key, &pair, &pair });

        head->last->next = &pair;
        head->last = &pair;
        pair.next = nullptr;
    }
}

http_RequestStatus http_IO::ParseRequest()
{
    Span<char> intro = {};
    bool keepalive = false;
    Span<const char> host = {};
    int64_t body_len = 0;
    bool explicit_len = false;
    bool known_addr = (daemon->addr_mode == http_AddressMode::Socket);

    // Find end of request headers (CRLF+CRLF)
    {
        uint8_t *end = (uint8_t *)MemMem(incoming.buf.ptr + incoming.pos, incoming.buf.len - incoming.pos, "\r\n\r\n", 4);

        if (!end) {
            if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
                SendAbort(431, "Excessive request size");
                return http_RequestStatus::Shutdown;
            }

            incoming.pos = std::max((Size)0, incoming.buf.len - 3);
            return http_RequestStatus::Busy;
        }

        intro = MakeSpan((char *)incoming.buf.ptr, end - incoming.buf.ptr);
        incoming.pos = end - incoming.buf.ptr + 4;

        if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
            SendAbort(431, "Excessive request size");
            return http_RequestStatus::Shutdown;
        }
    }

    // Parse request line
    {
        Span<char> line = SplitStr(intro, "\r\n", &intro);

        Span<char> method = SplitStr(line, ' ', &line);
        Span<char> url = SplitStr(line, ' ', &line);
        Span<char> protocol = SplitStr(line, ' ', &line);

        if (line.len) {
            SendAbort(400, "Unexpected data after request line");
            return http_RequestStatus::Shutdown;
        }
        if (!method.len) {
            SendAbort(400, "Empty HTTP method");
            return http_RequestStatus::Shutdown;
        }
        if (!StartsWith(url, "/")) {
            SendAbort(400, "Request URL does not start with '/'");
            return http_RequestStatus::Shutdown;
        }
        if (url.len > daemon->max_url_len) {
            SendAbort(400, "Request URL is too long");
            return http_RequestStatus::Shutdown;
        }
        if (TestStr(protocol, "HTTP/1.0")) {
            request.version = 10;
            keepalive = false;
        } else if (TestStr(protocol, "HTTP/1.1")) {
            request.version = 11;
            keepalive = true;
        } else {
            SendAbort(400, "Invalid HTTP version");
            return http_RequestStatus::Shutdown;
        }

        if (TestStr(method, "HEAD")) {
            request.method = http_RequestMethod::Get;
            request.head = true;
        } else if (OptionToEnum(http_RequestMethodNames, method, &request.method)) {
            request.head = false;
        } else {
            SendAbort(405, "Unsupported HTTP method");
            return http_RequestStatus::Shutdown;
        }
        request.client_addr = addr;

        Span<char> query;
        Span<char> path = SplitStr(url, '?', &query);

        path.len = DecodePath(path);
        if (path.len < 0) {
            SendAbort(400, "Unsafe URL path");
            return http_RequestStatus::Shutdown;
        }
        path.ptr[path.len] = 0;
        request.path = path.ptr;

        if (PathContainsDotDot(request.path)) {
            SendAbort(403, "Unsafe URL containing '..' components");
            return http_RequestStatus::Shutdown;
        }

        if (!DecodeQuery(query, &request.values)) {
            SendAbort(400, "Unsafe URL query string");
            return http_RequestStatus::Shutdown;
        }
    }

    // Parse headers
    while (intro.len) {
        Span<char> line = SplitStr(intro, "\r\n", &intro);

        Span<char> key = SplitStr(line, ':', &line);
        Span<char> value = TrimStr(line);

        if (line.ptr == key.end()) [[unlikely]] {
            SendAbort(400, "Missing colon in header line");
            return http_RequestStatus::Shutdown;
        }
        if (!key.len || !IsFieldKeyValid(key)) [[unlikely]] {
            SendAbort(400, "Malformed header key");
            return http_RequestStatus::Shutdown;
        }
        if (!IsFieldValueValid(value)) {
            SendAbort(400, "Malformed header value");
            return http_RequestStatus::Shutdown;
        }

        // Canonicalize header key
        bool upper = true;
        for (char &c: key.As<char>()) {
            c = upper ? UpperAscii(c) : LowerAscii(c);
            upper = (c == '-');
        }

        // Append to list of headers
        {
            key.ptr[key.len] = 0;
            value.ptr[value.len] = 0;

            if (request.headers.len >= daemon->max_request_headers) [[unlikely]] {
                SendAbort(431, "Too many headers");
                return http_RequestStatus::Shutdown;
            }

            request.headers.Append({ key.ptr, value.ptr, nullptr });
        }

        // Handle special headers
        if (key == "Host") {
            if (host.len && !TestStr(host, value)) [[unlikely]] {
                SendAbort(400, "Refusing mismatched Host values");
                return http_RequestStatus::Shutdown;
            }

            host = value;
        } else if (key == "Cookie") {
            Span<char> remain = value;

            while (remain.len) {
                Span<char> name = TrimStr(SplitStr(remain, '=', &remain));
                Span<char> value = TrimStr(SplitStr(remain, ';', &remain));

                if (!IsFieldKeyValid(name)) [[unlikely]] {
                    SendAbort(400, "Malformed cookie name");
                    return http_RequestStatus::Shutdown;
                }
                if (!IsFieldValueValid(value)) [[unlikely]] {
                    SendAbort(400, "Malformed cookie value");
                    return http_RequestStatus::Shutdown;
                }

                name.ptr[name.len] = 0;
                value.ptr[value.len] = 0;

                if (request.cookies.len >= daemon->max_request_cookies) [[unlikely]] {
                    SendAbort(431, "Too many cookies");
                    return http_RequestStatus::Shutdown;
                }

                request.cookies.Append({ name.ptr, value.ptr, nullptr });
            }
        } else if (key == "Connection") {
            keepalive = !TestStrI(value, "close");
        } else if (key == "Content-Length") {
            int64_t len;
            if (!ParseInt(value, &len, K_DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log) || len < 0) [[unlikely]] {
                SendAbort(400, "Invalid Content-Length value");
                return http_RequestStatus::Shutdown;
            }

            if (len && request.method == http_RequestMethod::Get) [[unlikely]] {
                SendAbort(400, "Refusing to process GET request with body");
                return http_RequestStatus::Shutdown;
            }
            if (explicit_len && len != body_len) [[unlikely]] {
                SendAbort(400, "Refusing mismatched Content-Length values");
                return http_RequestStatus::Shutdown;
            }

            body_len = len;
            explicit_len = true;
        } else if (daemon->addr_mode == http_AddressMode::XForwardedFor && key == "X-Forwarded-For") {
            Span<const char> trimmed = TrimStr(SplitStrReverse(value, ','));

            if (!trimmed.len) [[unlikely]] {
                SendAbort(400, "Empty client address in X-Forwarded-For header");
                return http_RequestStatus::Shutdown;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                SendAbort(400, "Excessively long client address in X-Forwarded-For header");
                return http_RequestStatus::Shutdown;
            }

            known_addr = true;
        } else if (daemon->addr_mode == http_AddressMode::XRealIP && key == "X-Real-Ip") {
            Span<const char> trimmed = TrimStr(value);

            if (!trimmed.len) [[unlikely]] {
                SendAbort(400, "Empty client address in X-Real-Ip header");
                return http_RequestStatus::Shutdown;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                SendAbort(400, "Excessively long client address in X-Real-Ip header");
                return http_RequestStatus::Shutdown;
            }

            known_addr = true;
        } else if (key == "Transfer-Encoding") [[unlikely]] {
            SendAbort(501, "Requests with Transfer-Encoding are not supported");
            return http_RequestStatus::Shutdown;
        } else if (key == "Content-Encoding") [[unlikely]] {
            SendAbort(415, "Requests with Content-Encoding are not supported");
            return http_RequestStatus::Shutdown;
        }
    }

    if (request.version >= 11 && !host.len) [[unlikely]] {
        SendAbort(400, "Missing Host header in HTTP/1.1 request");
        return http_RequestStatus::Shutdown;
    }
    if (!known_addr) [[unlikely]] {
        char buf[128];
        Fmt(buf, "Missing expected address header '%1'", http_AddressModeNames[(int)daemon->addr_mode]);

        SendAbort(400, buf);
        return http_RequestStatus::Shutdown;
    }

    // Map keys for faster access
    MapKeys(request.values, &request.values_map);
    MapKeys(request.headers, &request.headers_map);
    MapKeys(request.cookies, &request.cookies_map);

    // Set at the end so any error before would lead to "Connection: close"
    request.body_len = body_len;
    request.keepalive = keepalive;

    SetTimeout(GetMonotonicClock() + daemon->idle_timeout);

    return http_RequestStatus::Ready;
}

void http_IO::SendAbort(int status, const char *msg)
{
    // This should only ever be called by ParseRequest!
    // It may be called by a dispatcher thread, so it must not block. The easy way to do this
    // is to keep the socket in non-blocking mode.
    // If the buffer is full (unlikely because that's pretty much the first thing we write to it),
    // it just does not get sent. Since this is only called for request parse errors, it's okay!.
    // Side note: the peer address gets used in the log instead of client_addr which is
    // not trusted at this stage.

    K_ASSERT(!request.keepalive);

#if defined(K_DEBUG)
    // We want to keep the debug-only source code context
    {
        PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
            char ctx_buf[512];
            Fmt(ctx_buf, "%1%2: ", ctx ? ctx : "", addr);

            func(level, ctx_buf, msg);
        });
        K_DEFER { PopLogFilter(); };

        LogError("%1", msg);
    }
#else
    Log(LogLevel::Error, addr, "%1", msg);
#endif

    incoming.buf.len = 0;
    incoming.pos = 0;

    daemon->StartWrite(socket, false);
    K_DEFER { daemon->EndWrite(socket); };

    Span<const char> body = msg;
    Span<const char> intro = PrepareResponse(status, CompressionType::None, body.len, false);

    if (!WriteDirect(intro.As<const uint8_t>()))
        return;
    if (!request.head && !WriteDirect(body.As<const uint8_t>()))
        return;
}

bool http_IO::StartResponse()
{
    K_ASSERT(socket);
    K_ASSERT(!response.started);

    if (response.started) [[unlikely]] {
        LogWarning("Send multiple HTTP responses (bug)");

        request.keepalive = false;
        return false;
    }

    daemon->StartWrite(socket, true);

    // Unfortunately, we need to discard the whole body before we can respond, even if it
    // was not used / we don't care about it. But do it within limits, and ignore otherwise.
    {
        int64_t remaining = request.body_len - incoming.read;

        if (remaining) {
            int64_t max = Kibibytes(64);
            bool full = remaining < max;
            int64_t discard = incoming.read + (full ? remaining : max);

            while (incoming.read < discard) {
                uint8_t buf[65535];

                if (ReadDirect(buf) < 0) {
                    request.keepalive = false;
                    return false;
                }
            }

            // Avoid desync
            request.keepalive &= full;
        }
    }

    response.started = true;
    SetTimeout(GetMonotonicClock() + daemon->send_timeout);

    return true;
}

Span<const char> http_IO::PrepareResponse(int status, CompressionType encoding, int64_t len, bool chunked)
{
    K_ASSERT(len < 0 || !chunked);

    HeapArray<char> buf(&allocator);
    buf.Grow(Kibibytes(2));

    const char *protocol = (request.version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = http_ErrorMessages.FindValue(status, "Unknown");
    const char *connection = request.keepalive ? "keep-alive" : "close";

    Fmt(&buf, "%1 %2 %3\r\nConnection: %4\r\n", protocol, status, details, connection);

    switch (encoding) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: { Fmt(&buf, "Content-Encoding: deflate\r\n"); } break;
        case CompressionType::Gzip: { Fmt(&buf, "Content-Encoding: gzip\r\n"); } break;
        case CompressionType::Brotli: { Fmt(&buf, "Content-Encoding: br\r\n"); } break;
        case CompressionType::Zstd: { Fmt(&buf, "Content-Encoding: zstd\r\n"); } break;
    }

    for (const http_KeyValue &header: response.headers) {
        // Header values are a mess, so the caller is responsible for proper encoding
        // But we still want to avoid the possibility of response splitting.

        Fmt(&buf, "%1: %2\r\n", header.key, FmtHeaderValue(header.value));
    }

    if (len >= 0) {
        Fmt(&buf, "Content-Length: %1\r\n\r\n", len);
    } else if (chunked) {
        Fmt(&buf, "Transfer-Encoding: chunked\r\n\r\n");
    } else {
        buf.Append("\r\n");
    }

    return buf.TrimAndLeak();
}

Size http_IO::ReadDirect(Span<uint8_t> buf)
{
    buf.len = std::min((int64_t)buf.len, request.body_len - incoming.read);

    Size start_len = buf.len;

    if (incoming.pos < incoming.buf.len) {
        Size available = incoming.buf.len - incoming.pos;
        Size copy_len = std::min(buf.len, available);

        MemCpy(buf.ptr, incoming.buf.ptr + incoming.pos, copy_len);
        incoming.pos += copy_len;

        buf.ptr += copy_len;
        buf.len -= copy_len;
    }

    while (buf.len) {
        Size bytes = daemon->ReadSocket(socket, buf);

        if (bytes < 0)
            return -1;
        if (!bytes) {
            LogError("Connection closed unexpectedly");
            return -1;
        }

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    incoming.read += start_len - buf.len;
    return start_len - buf.len;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    if (!data.len) {
        daemon->EndWrite(socket);
        return true;
    }

    if (!daemon->WriteSocket(socket, data)) {
        request.keepalive = false;
        return false;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    if (!data.len) {
        uint8_t end[5] = { '0', '\r', '\n', '\r', '\n' };

        if (!daemon->WriteSocket(socket, end)) {
            request.keepalive = false;
            return false;
        }
        daemon->EndWrite(socket);

        return true;
    }

    if (data.len > (Size)16 * 0xFFFF) [[unlikely]] {
        do {
            Size take = std::min((Size)16 * 0xFFFF, data.len);
            Span<const uint8_t> frag = data.Take(0, take);

            if (!WriteChunked(frag))
                return false;

            data.ptr += take;
            data.len -= take;
        } while (data.len);

        return true;
    }

    uint8_t full[8] = { '\r', '\n', 'F', 'F', 'F', 'F', '\r', '\n' };
    uint8_t last[8] = { '\r', '\n', 0, 0, 0, 0, '\r', '\n' };

    LocalArray<Span<const uint8_t>, 2 * 16 + 1> parts;

    while (data.len >= 0xFFFF) {
        parts.Append(MakeSpan(full, K_SIZE(full)));
        parts.Append(MakeSpan(data.ptr, 0xFFFF));

        data.ptr += 0xFFFF;
        data.len -= 0xFFFF;
    }

    if (data.len) {
        static const char literals[] = "0123456789ABCDEF";

        last[2] = literals[((size_t)data.len >> 12) & 0xF];
        last[3] = literals[((size_t)data.len >> 8) & 0xF];
        last[4] = literals[((size_t)data.len >> 4) & 0xF];
        last[5] = literals[((size_t)data.len >> 0) & 0xF];

        parts.Append(MakeSpan(last, K_SIZE(last)));
        parts.Append(MakeSpan(data.ptr, data.len));
    }

    parts[0].ptr += 2;
    parts[0].len -= 2;
    parts.Append(MakeSpan(full, 2));

    return daemon->WriteSocket(socket, parts);
}

bool http_IO::Rearm(int64_t now)
{
    bool keepalive = request.keepalive && (now >= 0);

    if (keepalive) {
        // Make sure the client gets some extra time when in Keep-Alive time to avoid
        // abrupt disconnection once we have sent "Connection: keep-alive" to the client.
        int64_t keepalive_timeout = std::max(socket_start + daemon->keepalive_time, now + 5000);

        SetTimeout(keepalive_timeout);

        MemMove(incoming.buf.ptr, incoming.buf.ptr + incoming.pos, incoming.buf.len - incoming.pos);
        incoming.buf.len -= incoming.pos;
    } else {
        SetTimeout(now + 5000);

        if (incoming.buf.capacity < Kibibytes(8)) {
            incoming.buf.len = 0;
        } else {
            incoming.buf.Clear();
        }
    }

    incoming.pos = 0;
    incoming.read = 0;
    incoming.reading = false;

    request.keepalive = false;
    request.values.RemoveFrom(0);
    request.headers.RemoveFrom(0);
    request.cookies.RemoveFrom(0);
    request.values_map.RemoveAll();
    request.headers_map.RemoveAll();
    request.cookies_map.RemoveAll();
    request.body_len = 0;

    response.headers.RemoveFrom(0);
    response.started = false;
    last_err = nullptr;

    ws_opcode = 0;

    if (keepalive) {
        allocator.Reset();
    } else {
        allocator.ReleaseAll();
    }

    return keepalive;
}

bool http_IO::IsBusy() const
{
    if (!incoming.buf.len)
        return false;
    if (incoming.reading && incoming.read == request.body_len)
        return false;

    return true;
}

}
