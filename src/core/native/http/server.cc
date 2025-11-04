// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
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
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
#endif

namespace K {

K_CONSTINIT ConstMap<128, int, const char *> http_ErrorMessages = {
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

    if (idle_timeout < 1000) {
        LogError("HTTP IdleTimeout must be >= 1 sec");
        return false;
    }
    if (keepalive_time && keepalive_time < 5000) {
        LogError("HTTP KeepAliveTime must be >= 5 sec (or Disabled)");
        return false;
    }
    if (send_timeout < 10000) {
        LogError("HTTP SendTimeout must be >= 10 sec");
        return false;
    }
    if (stop_timeout < 1000) {
        LogError("HTTP StopTimeout must be >= 1 sec");
        return false;
    }

    if (max_request_size < 1024) {
        LogError("MaxRequestSize must be >= 1 kB");
        valid = false;
    }
    if (max_url_len < 512) {
        LogError("MaxUrlLength must be >= 512 B");
        valid = false;
    }
    if (max_request_cookies < 16) {
        LogError("MaxRequestHeaders must be >= 16");
        valid = false;
    }
    if (max_request_cookies < 0) {
        LogError("MaxRequestCookies must be >= 0");
        valid = false;
    }

    return valid;
}

static void AllowPortReuse([[maybe_unused]] int sock)
{
#if defined(SO_REUSEPORT_LB)
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &reuse, sizeof(reuse));
#elif defined(SO_REUSEPORT)
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
 #endif
}

static int CreateListenSocket(const http_Config &config, bool first)
{
    int sock = CreateSocket(config.sock_type, SOCK_STREAM);
    if (sock < 0)
        return -1;
    K_DEFER_N(err_guard) { CloseSocket(sock); };

    if (!first) {
        // Set SO_REUSEPORT after first connection, so that two HTTP serving processes don't end up
        // overlapping each other.
        AllowPortReuse(sock);
    }

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
        // The bind succeeded, we know that no other process is using this port. Let the next sockets
        // reuse this port.
        AllowPortReuse(sock);
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

    SetDescriptorNonBlock(sock, true);

    err_guard.Disable();
    return sock;
}

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    K_ASSERT(!listeners.len);

    if (!config.Validate())
        return false;

    if (config.addr_mode == http_AddressMode::Socket) {
        LogWarning("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-IP "
                   "if you run this behind a reverse proxy that sets one of these headers.");
    }

    // Copy main confg values
    sock_type = config.sock_type;
    addr_mode = config.addr_mode;
    idle_timeout = config.idle_timeout;
    keepalive_time = config.keepalive_time;
    send_timeout = config.send_timeout;
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

    workers = 2 * GetCoreCount();

    for (Size i = 0; i < workers; i++) {
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

bool http_Daemon::InitConfig(const http_Config &config)
{
    if (!config.Validate())
        return false;

    if (config.addr_mode == http_AddressMode::Socket) {
        LogWarning("You may want to %!.._set HTTP.ClientAddress%!0 to X-Forwarded-For or X-Real-IP "
                   "if you run this behind a reverse proxy that sets one of these headers.");
    }

    sock_type = config.sock_type;
    addr_mode = config.addr_mode;

    idle_timeout = config.idle_timeout;
    keepalive_time = config.keepalive_time;
    send_timeout = config.send_timeout;
    stop_timeout = config.stop_timeout;

    max_request_size = config.max_request_size;
    max_url_len = config.max_url_len;
    max_request_headers = config.max_request_headers;
    max_request_cookies = config.max_request_cookies;

    return true;
}

void http_Daemon::RunHandler(http_IO *client, int64_t now)
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
        ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
        '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~'
    };

    bool valid = std::all_of(key.begin(), key.end(), [](char c) { return ValidCharacters.Test((uint8_t)c); });
    return valid;
}

[[maybe_unused]] static bool IsHeaderKeyValid(const char *key)
{
    bool upper = true;

    for (Size i = 0; key[i]; i++) {
        int c = key[i];

        bool valid = upper ? (c == UpperAscii(c)) : (c == LowerAscii(c));
        if (!valid)
            return false;

        upper = (c == '-');
    }

    bool empty = !key[0];
    return !empty;
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

bool http_IO::OpenForRead(Size max_len, StreamReader *out_st)
{
    K_ASSERT(socket);
    K_ASSERT(!incoming.reading);

    // Safety checks
    if (request.GetHeaderValue("Content-Encoding")) {
        LogError("Refusing request body with Content-Encoding header");
        SendError(400);
        return false;
    }
    if (max_len >= 0 && request.body_len > max_len) {
        LogError("HTTP body is too big (max = %1)", FmtDiskSize(max_len));
        SendError(413);
        return false;
    }

    daemon->StartRead(socket);

    incoming.reading = true;
    timeout_at = GetMonotonicTime() + daemon->send_timeout;

    bool success = out_st->Open([this](Span<uint8_t> out_buf) { return ReadDirect(out_buf); }, "<http>");
    K_ASSERT(success);

    // Additional precaution
    out_st->SetReadLimit(max_len);

    return true;
}

void http_IO::AddHeader(Span<const char> key, Span<const char> value)
{
    K_ASSERT(!response.started);

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
        case CompressionType::LZ4: { K_UNREACHABLE(); } break;
        case CompressionType::Zstd: { AddHeader("Content-Encoding", "zstd"); } break;
    }
}

void http_IO::AddCookieHeader(const char *path, const char *name, const char *value, unsigned int flags)
{
    LocalArray<char, 1024> buf;

    if (value) {
        buf.len = Fmt(buf.data, "%1=%2; Path=%3;", name, value, path).len;
    } else {
        buf.len = Fmt(buf.data, "%1=; Path=%2; Max-Age=0;", name, path).len;
    }

    K_ASSERT(buf.Available() >= 128);

    buf.len += Fmt(buf.TakeAvailable(), " SameSite=Strict;").len;
    if (flags & (int)http_CookieFlag::HttpOnly) {
        buf.len += Fmt(buf.TakeAvailable(), " HttpOnly;").len;
    }
    if (flags & (int)http_CookieFlag::Secure) {
        buf.len += Fmt(buf.TakeAvailable(), " Secure;").len;
    }

    AddHeader("Set-Cookie", buf.data);
}

void http_IO::AddCachingHeaders(int64_t max_age, const char *etag)
{
    K_ASSERT(max_age >= 0);

#if defined(K_DEBUG)
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
        SendError(406);
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
        SendError(406);
        return false;
    }
}

bool http_IO::OpenForWrite(int status, CompressionType encoding, int64_t len, StreamWriter *out_st)
{
    K_ASSERT(socket);
    K_ASSERT(!response.started);
    K_ASSERT(!request.headers_only);

    daemon->StartWrite(socket);

    // Unfortunately, we need to discard the whole body before we can respond, even if it
    // was not used / we don't care about it. But do it within limits, and ignore otherwise.
    {
        int64_t remaining = request.body_len - incoming.read;
        int64_t discard = incoming.read + std::min(remaining, (int64_t)Mebibytes(32));

        while (incoming.read < discard) {
            uint8_t buf[65535];

            if (ReadDirect(buf) < 0)
                return false;
        }
    }

    response.started = true;

    // Don't allow Keep-Alive with HTTP/1.0 when chunked encoding is used
    request.keepalive &= (len >= 0 || request.version >= 11);

    Span<const char> intro = PrepareResponse(status, encoding, len);
    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };

    out_st->Open(write, "<http>");
    out_st->Write(intro);

    if (len >= 0) {
        if (encoding == CompressionType::None)
            return true;

        out_st->Close();
        return out_st->Open(write, "<http>", 0, encoding);
    } else {
        const auto chunk = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };

        out_st->Close();
        return out_st->Open(chunk, "<http>", 0, encoding);
    }
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(StreamWriter *)> func)
{
    K_ASSERT(socket);
    K_ASSERT(!response.started);

    // HEAD quick path
    if (request.headers_only) {
        daemon->StartWrite(socket);
        response.started = true;

        const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
        StreamWriter writer(write, "<http>");

        Span<const char> intro = PrepareResponse(status, encoding, len);
        writer.Write(intro);

        request.keepalive &= writer.Close();

        return;
    }

    StreamWriter writer;
    if (!OpenForWrite(status, encoding, len, &writer)) [[unlikely]]
        return;

    request.keepalive &= func(&writer);
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

        if (request.headers_only) {
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
    K_DEFER_N(err_guard) { CloseDescriptor(fd); };

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

    err_guard.Disable();
    SendFile(status, fd, file_info.size);
}

void http_IO::ExtendTimeout(int timeout)
{
    int64_t now = GetMonotonicTime();
    timeout_at = now + timeout;
}

bool http_IO::Init(http_Socket *socket, int64_t start, struct sockaddr *sa)
{
    this->socket = socket;

    if (daemon->addr_mode == http_AddressMode::Socket) {
        switch (sa->sa_family) {
            case AF_INET: {
                void *ptr = &((sockaddr_in *)sa)->sin_addr;

                if (!inet_ntop(AF_INET, ptr, addr, K_SIZE(addr))) [[unlikely]] {
                    LogError("Cannot convert IPv4 address to text");
                    return false;
                }
            } break;

            case AF_INET6: {
#if !defined(_WIN32)
                K_ASSERT(K_SIZE(addr) >= INET6_ADDRSTRLEN + 2);
#endif

                void *ptr = &((sockaddr_in6 *)sa)->sin6_addr;

                if (!inet_ntop(AF_INET6, ptr, addr, K_SIZE(addr))) [[unlikely]] {
                    LogError("Cannot convert IPv6 address to text");
                    return false;
                }

                if (StartsWith(addr, "::ffff:") || StartsWith(addr, "::FFFF:")) {
                    // Not supposed to even go near the limit, but make sure!
                    Size move = std::min((Size)strlen(addr + 7) + 1, K_SIZE(addr) - 8);
                    MemMove(addr, addr + 7, move);
                }
            } break;

            case AF_UNIX: { CopyString("unix", addr); } break;

            default: { K_UNREACHABLE(); } break;
        }
    }

    socket_start = start;
    timeout_at = start + daemon->idle_timeout;

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

        if (IsAsciiControl(str[i])) [[unlikely]] {
            LogError("Unexpected control character in HTTP request line");
            return -1;
        }

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

        if (IsAsciiControl(str[i])) [[unlikely]] {
            LogError("Unexpected control character in HTTP request line");
            return -1;
        }

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
            if (key.len < 0)
                return false;
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
        http_KeyHead *head = out_map->TrySet({ pair.key, &pair, &pair });

        head->last->next = &pair;
        head->last = &pair;
        pair.next = nullptr;
    }
}

http_RequestStatus http_IO::ParseRequest()
{
    Span<char> intro = {};
    bool keepalive = false;
    bool known_addr = (daemon->addr_mode == http_AddressMode::Socket);

    // Find end of request headers (CRLF+CRLF)
    {
        uint8_t *end = (uint8_t *)MemMem(incoming.buf.ptr + incoming.pos, incoming.buf.len - incoming.pos, "\r\n\r\n", 4);

        if (!end) {
            incoming.pos = std::max((Size)0, incoming.buf.len - 3);
            return http_RequestStatus::Busy;
        }

        intro = MakeSpan((char *)incoming.buf.ptr, end - incoming.buf.ptr);
        incoming.pos = end - incoming.buf.ptr + 4;

        if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
            LogError("Excessive request size");
            SendError(413);
            return http_RequestStatus::Close;
        }
    }

    // Parse request line
    {
        Span<char> line = SplitStrLine(intro, &intro);

        Span<char> method = SplitStr(line, ' ', &line);
        Span<char> url = SplitStr(line, ' ', &line);
        Span<char> protocol = SplitStr(line, ' ', &line);

        if (line.len) {
            LogError("Unexpected data after request line");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (!method.len) {
            LogError("Empty HTTP method");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (!StartsWith(url, "/")) {
            LogError("Request URL does not start with '/'");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (url.len > daemon->max_url_len) {
            LogError("Request URL is too long");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (TestStr(protocol, "HTTP/1.0")) {
            request.version = 10;
            keepalive = false;
        } else if (TestStr(protocol, "HTTP/1.1")) {
            request.version = 11;
            keepalive = true;
        } else {
            LogError("Invalid HTTP version");
            SendError(400);
            return http_RequestStatus::Close;
        }

        if (TestStr(method, "HEAD")) {
            request.method = http_RequestMethod::Get;
            request.headers_only = true;
        } else if (OptionToEnum(http_RequestMethodNames, method, &request.method)) {
            request.headers_only = false;
        } else {
            LogError("Unsupported HTTP method '%1'", method);
            SendError(405);
            return http_RequestStatus::Close;
        }
        request.client_addr = addr;

        Span<char> query;
        Span<char> path = SplitStr(url, '?', &query);

        path.len = DecodePath(path);
        if (path.len < 0) {
            SendError(400);
            return http_RequestStatus::Close;
        }
        path.ptr[path.len] = 0;
        request.path = path.ptr;

        if (PathContainsDotDot(request.path)) {
            LogError("Unsafe URL containing '..' components");
            SendError(403);
            return http_RequestStatus::Close;
        }

        if (!DecodeQuery(query, &request.values)) {
            SendError(400);
            return http_RequestStatus::Close;
        }
    }

    // Parse headers
    while (intro.len) {
        Span<char> line = SplitStrLine(intro, &intro);

        Span<char> key = SplitStr(line, ':', &line);
        Span<char> value = TrimStr(line);

        if (line.ptr == key.end()) [[unlikely]] {
            LogError("Missing colon in header line");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (!key.len || !IsFieldKeyValid(key)) [[unlikely]] {
            LogError("Malformed header key");
            SendError(400);
            return http_RequestStatus::Close;
        }
        if (!IsFieldValueValid(value)) {
            LogError("Malformed header value");
            SendError(400);
            return http_RequestStatus::Close;
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
                LogError("Too many headers, server limit is %1", daemon->max_request_headers);
                SendError(413);
                return http_RequestStatus::Close;
            }

            request.headers.Append({ key.ptr, value.ptr, nullptr });
        }

        // Handle special headers
        if (TestStr(key, "Cookie")) {
            Span<char> remain = value;

            while (remain.len) {
                Span<char> name = TrimStr(SplitStr(remain, '=', &remain));
                Span<char> value = TrimStr(SplitStr(remain, ';', &remain));

                if (!IsFieldKeyValid(name)) [[unlikely]] {
                    LogError("Malformed cookie name");
                    SendError(400);
                    return http_RequestStatus::Close;
                }
                if (!IsFieldValueValid(value)) [[unlikely]] {
                    LogError("Malformed cookie value");
                    SendError(400);
                    return http_RequestStatus::Close;
                }

                name.ptr[name.len] = 0;
                value.ptr[value.len] = 0;

                if (request.cookies.len >= daemon->max_request_cookies) [[unlikely]] {
                    LogError("Too many cookies, server limit is %1", daemon->max_request_cookies);
                    SendError(413);
                    return http_RequestStatus::Close;
                }

                request.cookies.Append({ name.ptr, value.ptr, nullptr });
            }
        } else if (TestStr(key, "Connection")) {
            keepalive = !TestStrI(value, "close");
        } else if(TestStr(key, "Content-Length")) {
            if (!ParseInt(value, &request.body_len)) [[unlikely]] {
                SendError(400);
                return http_RequestStatus::Close;
            }

            if (request.body_len < 0) [[unlikely]] {
                LogError("Negative Content-Length is not valid");
                SendError(400);
                return http_RequestStatus::Close;
            }
            if (request.body_len && request.method == http_RequestMethod::Get) [[unlikely]] {
                LogError("Refusing to process GET request with body");
                SendError(400);
                return http_RequestStatus::Close;
            }
        } else if (daemon->addr_mode == http_AddressMode::XForwardedFor && TestStr(key, "X-Forwarded-For")) {
            Span<const char> trimmed = TrimStr(SplitStr(value, ','));

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                SendError(400);
                return http_RequestStatus::Close;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                SendError(400);
                return http_RequestStatus::Close;
            }

            known_addr = true;
        } else if (daemon->addr_mode == http_AddressMode::XRealIP && TestStr(key, "X-Real-IP")) {
            Span<const char> trimmed = TrimStr(value);

            if (!trimmed.len) [[unlikely]] {
                LogError("Empty client address in X-Forwarded-For header");
                SendError(400);
                return http_RequestStatus::Close;
            }
            if (!CopyString(trimmed, addr)) [[unlikely]] {
                LogError("Excessively long client address in X-Forwarded-For header");
                SendError(400);
                return http_RequestStatus::Close;
            }

            known_addr = true;
        } else if (TestStr(key, "Transfer-Encoding")) {
            LogError("Requests with Transfer-Encoding are not supported");
            SendError(501);
            return http_RequestStatus::Close;
        }
    }

    if (!known_addr) [[unlikely]] {
        LogError("Missing expected %1 address header", http_AddressModeNames[(int)daemon->addr_mode]);
        SendError(400);
        return http_RequestStatus::Close;
    }

    // Map keys for faster access
    MapKeys(request.values, &request.values_map);
    MapKeys(request.headers, &request.headers_map);
    MapKeys(request.cookies, &request.cookies_map);

    // Set at the end so any error before would lead to "Connection: close"
    request.keepalive = keepalive;

    return http_RequestStatus::Ready;
}

Span<const char> http_IO::PrepareResponse(int status, CompressionType encoding, int64_t len)
{
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
        case CompressionType::LZ4: { K_UNREACHABLE(); } break;
        case CompressionType::Zstd: { Fmt(&buf, "Content-Encoding: zstd\r\n"); } break;
    }

    for (const http_KeyValue &header: response.headers) {
        Fmt(&buf, "%1: %2\r\n", header.key, header.value);
    }

    if (len >= 0) {
        Fmt(&buf, "Content-Length: %1\r\n\r\n", len);
    } else {
        Fmt(&buf, "Transfer-Encoding: chunked\r\n\r\n");
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
        int64_t keepalive_timeout = socket_start + daemon->keepalive_time;

        // Make sure the client gets some extra time when in Keep-Alive time to avoid
        // abrupt disconnection once we have sent "Connection: keep-alive" to the client.
        timeout_at = std::max(keepalive_timeout, now + 5000);

        MemMove(incoming.buf.ptr, incoming.buf.ptr + incoming.pos, incoming.buf.len - incoming.pos);
        incoming.buf.len -= incoming.pos;
    } else {
        timeout_at = now + 5000;
        incoming.buf.len = 0;
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
