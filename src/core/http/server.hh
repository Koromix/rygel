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

#pragma once

#include "src/core/base/base.hh"

struct sockaddr;

namespace RG {

enum class http_ClientAddressMode {
    Socket,
    XForwardedFor,
    XRealIP
};
static const char *const http_ClientAddressModeNames[] = {
    "Socket",
    "X-Forwarded-For",
    "X-Real-IP"
};

extern RG_CONSTINIT ConstMap<128, int, const char *> http_ErrorMessages;

struct http_Config {
#if defined(__OpenBSD__)
    SocketType sock_type = SocketType::IPv4;
#else
    SocketType sock_type = SocketType::Dual;
#endif
    int port = 8888;
    const char *unix_path = nullptr;

    http_ClientAddressMode addr_mode = http_ClientAddressMode::Socket;

    int idle_timeout = 10000;
    int keepalive_time = 20000;

    Size max_request_size = Kilobytes(40);
    Size max_url_len = Kilobytes(20);
    int max_request_headers = 64;
    int max_request_cookies = 64;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});
    bool SetPortOrPath(Span<const char> str);
    bool Validate() const;

    http_Config() = default;
    http_Config(int port) : port(port) {}
};

struct http_RequestInfo;
class http_IO;

class http_Dispatcher;
struct http_Socket;

class http_Daemon {
    RG_DELETE_COPY(http_Daemon)

    HeapArray<int> listeners;

#if defined(_WIN32)
    void *iocp = nullptr; // HANDLE
#endif

    SocketType sock_type;
    http_ClientAddressMode addr_mode;

    int idle_timeout;
    int keepalive_time;

    Size max_request_size;
    Size max_url_len;
    int max_request_headers;
    int max_request_cookies;

    Async *async = nullptr;
    http_Dispatcher *dispatcher = nullptr;

    std::function<void(const http_RequestInfo &request, http_IO *io)> handle_func;

public:
    http_Daemon() {}
    ~http_Daemon() { Stop(); }

    bool Bind(const http_Config &config, bool log_addr = true);
    bool Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func);

    void Stop();

private:
    bool InitConfig(const http_Config &config);

    void RunHandler(http_IO *client);

    friend class http_Dispatcher;
    friend class http_IO;
};

enum class http_RequestMethod {
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Options
};
static const char *const http_RequestMethodNames[] = {
    "GET",
    "POST",
    "PUT",
    "PATCH",
    "DELETE",
    "OPTIONS"
};

struct http_KeyValue {
    const char *key;
    const char *value;
};

struct http_RequestInfo {
    int version = 10;
    bool keepalive = false;
    http_RequestMethod method = http_RequestMethod::Get;
    bool headers_only = false;
    const char *client_addr = nullptr;
    const char *path = nullptr;

    HeapArray<http_KeyValue> values;
    HeapArray<http_KeyValue> headers;
    HeapArray<http_KeyValue> cookies;

    const char *FindGetValue(const char *key) const;
    const char *FindHeader(const char *key) const;
    const char *FindCookie(const char *key) const;
};

class http_IO {
    RG_DELETE_COPY(http_IO)

    enum class PrepareStatus {
        Incomplete,
        Ready,
        Close
    };

    http_Daemon *daemon;

    http_Socket *socket;
    char addr[65];

    int64_t socket_start;
    int64_t request_start;

    struct {
        HeapArray<uint8_t> buf;
        Size pos = 0;
        Span<char> intro = {};
        Span<uint8_t> extra = {};
    } incoming;

    http_RequestInfo request;

    const char *last_err = nullptr;

    struct {
        HeapArray<http_KeyValue> headers;
        HeapArray<std::function<void()>> finalizers;
        bool sent = false;
    } response;

    BlockAllocator allocator { Kibibytes(8) };

public:
    http_IO(http_Daemon *daemon) : daemon(daemon) { Rearm(0); }
    ~http_IO();

    RG::Allocator *Allocator() { return &allocator; }

    bool NegociateEncoding(CompressionType preferred, CompressionType *out_encoding);
    bool NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding);

    void AddHeader(Span<const char> key, Span<const char> value);
    void AddEncodingHeader(CompressionType encoding);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
    void AddCachingHeaders(int64_t max_age, const char *etag = nullptr);

    void Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func);
    void Send(int status, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
        { Send(status, CompressionType::None, len, func); }

    void SendEmpty(int status);
    void SendText(int status, Span<const char> text, const char *mimetype = "text/plain");
    void SendBinary(int status, Span<const uint8_t> data, const char *mimetype = nullptr);
    void SendAsset(int status, Span<const uint8_t> data, const char *mimetype = nullptr,
                   CompressionType src_encoding = CompressionType::None);
    void SendError(int status, const char *msg = nullptr);
    void SendFile(int status, const char *filename, const char *mimetype = nullptr);
    void SendFile(int status, int fd, int64_t len);

    void AddFinalizer(const std::function<void()> &func);

private:
    bool Init(http_Socket *socket, int64_t start, struct sockaddr *sa);
    bool InitAddress();

    PrepareStatus ParseRequest();
    Span<const char> PrepareResponse(int status, CompressionType encoding, int64_t len);

    bool WriteDirect(Span<const uint8_t> data);
    bool WriteChunked(Span<const uint8_t> data);

    void Rearm(int64_t start);

    bool IsPreparing() const;
    int64_t GetTimeout(int64_t now) const;

    friend class http_Daemon;
    friend class http_Dispatcher;
};

}
