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

namespace RG {

RG_CONSTINIT ConstMap<128, int, const char *> http_ErrorMessages = {
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
    RG_DEFER { PopLogFilter(); };

    handle_func(client->request, client);
}

const char *http_RequestInfo::FindHeader(const char *key) const
{
    for (const http_KeyValue &header: headers) {
        if (TestStr(header.key, key))
            return header.value;
    }

    return nullptr;
}

const char *http_RequestInfo::FindGetValue(const char *) const
{
    LogDebug("Not implemented");
    return nullptr;
}

const char *http_RequestInfo::FindCookie(const char *key) const
{
    for (const http_KeyValue &cookie: cookies) {
        if (TestStr(cookie.key, key))
            return cookie.value;
    }

    return nullptr;
}

void http_IO::AddHeader(Span<const char> key, Span<const char> value)
{
    RG_ASSERT(!response.sent);

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

void http_IO::SendEmpty(int status)
{
    Send(status, 0, [](int, StreamWriter *) { return true; });
}

void http_IO::SendText(int status, Span<const char> text, const char *mimetype)
{
    RG_ASSERT(mimetype);
    AddHeader("Content-Type", mimetype);

    Send(status, text.len, [&](int, StreamWriter *writer) { return writer->Write(text); });
}

void http_IO::SendBinary(int status, Span<const uint8_t> data, const char *mimetype)
{
    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, data.len, [&](int, StreamWriter *writer) { return writer->Write(data); });
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
            StreamReader reader(data, nullptr, src_encoding);
            Send(status, dest_encoding, -1, [&](int, StreamWriter *writer) { return SpliceStream(&reader, -1, writer); });
        }
    } else {
        if (mimetype) {
            AddHeader("Content-Type", mimetype);
        }

        SendBinary(status, data);
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

void http_IO::AddFinalizer(const std::function<void()> &func)
{
    RG_ASSERT(!response.sent);
    response.finalizers.Append(func);
}

bool http_IO::Init(int fd, int64_t start, struct sockaddr *sa)
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

    this->start = start;
    this->timeout = http_KeepAliveDelay;

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

static inline bool IsHeaderKeyValid(Span<const char> key)
{
    static RG_CONSTINIT Bitset<256> ValidCharacter = {
        0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2a, 0x2b, 0x2d, 0x2e, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
        0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7c, 0x7e
    };

    if (!key.len)
        return false;

    bool valid = std::all_of(key.begin(), key.end(), [](char c) { return ValidCharacter.Test((uint8_t)c); });
    return valid;
}

static inline bool IsHeaderValueValid(Span<const char> key)
{
    static RG_CONSTINIT Bitset<256> ValidCharacter = {
        0x09, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
        0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
        0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
        0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
        0x7f
    };

    bool valid = std::all_of(key.begin(), key.end(), [](char c) { return ValidCharacter.Test((uint8_t)c); });
    return valid;
}

bool http_IO::ParseRequest(Span<char> intro)
{
    bool keepalive = false;

    // Close connection if something fails here
    request.keepalive = false;

    // Parse request line
    {
        Span<char> line = SplitStrLine(intro, &intro);

        Span<char> method = SplitStr(line, ' ', &line);
        Span<char> url = SplitStr(line, ' ', &line);
        Span<char> protocol = SplitStr(line, ' ', &line);

        for (char &c: method) {
            c = UpperAscii(c);
        }

        if (!method.len) {
            LogError("Empty HTTP method");
            SendError(400);
            return false;
        }
        if (!StartsWith(url, "/")) {
            LogError("Invalid request URL");
            SendError(400);
            return false;
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
            return false;
        }
        if (line.len) {
            LogError("Unexpected data after request line");
            SendError(400);
            return false;
        }

        if (TestStr(method, "HEAD")) {
            request.method = http_RequestMethod::Get;
            request.headers_only = true;
        } else if (OptionToEnum(http_RequestMethodNames, method, &request.method)) {
            request.headers_only = false;
        } else {
            LogError("Unsupported HTTP method '%1'", method);
            SendError(405);
            return false;
        }
        request.client_addr = addr;

        Span<char> query;
        url = SplitStr(url, '?', &query);

        url.ptr[url.len] = 0;
        request.url = url.ptr;
    }

    // Parse headers
    while (intro.len) {
        Span<char> line = SplitStrLine(intro, &intro);

        Span<char> key = SplitStr(line, ':', &line);
        Span<char> value = TrimStr(line);

        if (line.ptr == key.end()) [[unlikely]] {
            LogError("Missing colon in header line");
            SendError(400);
            return false;
        }
        if (!key.len || !IsHeaderKeyValid(key)) [[unlikely]] {
            LogError("Malformed header key");
            SendError(400);
            return false;
        }
        if (!IsHeaderValueValid(value)) {
            LogError("Malformed header value");
            SendError(400);
            return false;
        }

        // Canonicalize header key
        bool upper = true;
        for (char &c: key.As<char>()) {
            c = upper ? UpperAscii(c) : LowerAscii(c);
            upper = (c == '-');
        }

        if (TestStr(key, "Cookie")) {
            Span<char> remain = value;

            while (remain.len) {
                Span<char> name = TrimStr(SplitStr(remain, '=', &remain));
                Span<char> value = TrimStr(SplitStr(remain, ';', &remain));

                name.ptr[name.len] = 0;
                value.ptr[value.len] = 0;

                request.cookies.Append({ name.ptr, value.ptr });
            }
        } else if (TestStr(key, "Connection")) {
            keepalive = !TestStrI(value, "close");
        } else {
            key.ptr[key.len] = 0;
            value.ptr[value.len] = 0;

            request.headers.Append({ key.ptr, value.ptr });
        }
    }

    request.keepalive = keepalive;
    return true;
}

void http_IO::Reset()
{
    for (const auto &finalize: response.finalizers) {
        finalize();
    }

    MemMove(incoming.buf.ptr, incoming.extra.ptr, incoming.extra.len);
    incoming.buf.RemoveFrom(incoming.extra.len);
    incoming.pos = 0;
    incoming.intro = {};
    incoming.extra = {};

    request.headers.RemoveFrom(0);
    request.cookies.RemoveFrom(0);

    response.headers.RemoveFrom(0);
    response.finalizers.RemoveFrom(0);
    response.sent = false;
    last_err = nullptr;

    allocator.Reset();

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
