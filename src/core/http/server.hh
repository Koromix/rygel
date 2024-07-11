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

struct http_Config {
#if defined(__OpenBSD__)
    SocketType sock_type = SocketType::IPv4;
#else
    SocketType sock_type = SocketType::Dual;
#endif
    int port = 8888;
    const char *unix_path = nullptr;

    http_ClientAddressMode addr_mode = http_ClientAddressMode::Socket;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});
    bool SetPortOrPath(Span<const char> str);
    bool Validate() const;

    http_Config() = default;
    http_Config(int port) : port(port) {}
};

struct http_RequestInfo;
class http_IO;

class http_Daemon {
    RG_DELETE_COPY(http_Daemon)

    class RequestHandler;

    int listen_fd = -1;
    http_ClientAddressMode addr_mode = http_ClientAddressMode::Socket;

    Async *async = nullptr;
    HeapArray<RequestHandler *> handlers;
    std::function<void(const http_RequestInfo &request, http_IO *io)> handle_func;

public:
    http_Daemon() {}
    ~http_Daemon() { Stop(); }

    bool Bind(const http_Config &config);
    bool Start(const http_Config &config,
               std::function<void(const http_RequestInfo &request, http_IO *io)> func,
               bool log_socket = true);

    void Stop();

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
    http_RequestMethod method = http_RequestMethod::Get;
    bool headers_only = false;
    const char *url = nullptr;
    HeapArray<http_KeyValue> headers;
    const char *client_addr = nullptr;

    const char *FindHeader(const char *key) const;
    const char *FindGetValue(const char *key) const;
    const char *FindCookie(const char *key) const;
};

class http_IO {
    RG_DELETE_COPY(http_IO)

    enum class PrepareStatus {
        Incomplete,
        Ready,
        Busy,
        Closed,
        Error
    };

    using RequestHandler = http_Daemon::RequestHandler;
    RequestHandler *handler;

    int fd;
    char addr[65];

    HeapArray<uint8_t> buf;
    Span<char> intro;
    Span<uint8_t> extra;

    std::atomic_bool ready;
    int version;
    bool keepalive;
    http_RequestInfo request;

    struct {
        HeapArray<http_KeyValue> headers;
        HeapArray<std::function<void()>> finalizers;
        bool sent = false;
    } response;

    int count;
    int64_t timeout;

    BlockAllocator allocator;

public:
    int Descriptor() const { return fd; }
    RG::Allocator *Allocator() { return &allocator; }

    bool NegociateEncoding(CompressionType preferred, CompressionType *out_encoding);
    bool NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding);

    void AddHeader(Span<const char> key, Span<const char> value);
    void AddEncodingHeader(CompressionType encoding);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
    void AddCachingHeaders(int64_t max_age, const char *etag = nullptr);

    void Send(int status, Size len, FunctionRef<void(int, StreamWriter *)> func);
    void SendText(int status, Span<const char> text, const char *mimetype = "text/plain");
    void SendBinary(int status, Span<const uint8_t> data, const char *mimetype = nullptr);
    bool SendAsset(int status, Span<const uint8_t> data, const char *mimetype = nullptr,
                   CompressionType compression_type = CompressionType::None);
    void SendError(int status, const char *details = nullptr);
    bool SendFile(int status, const char *filename, const char *mimetype = nullptr);
    void SendEmpty(int status);

    void AddFinalizer(const std::function<void()> &func);

private:
    http_IO(RequestHandler *handler, int fd);
    ~http_IO();

    PrepareStatus Prepare();
    bool InitAddress(http_ClientAddressMode addr_mode);

    void Reset();
    void Close();

    friend class http_Daemon;
    friend class http_Daemon::RequestHandler;
};

}
