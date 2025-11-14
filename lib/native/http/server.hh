// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

struct sockaddr;

namespace K {

enum class http_AddressMode {
    Socket,
    XForwardedFor,
    XRealIP
};
static const char *const http_AddressModeNames[] = {
    "Socket",
    "X-Forwarded-For",
    "X-Real-IP"
};

extern K_CONSTINIT ConstMap<128, int, const char *> http_ErrorMessages;

struct http_Config {
#if defined(__OpenBSD__)
    SocketType sock_type = SocketType::IPv4;
#else
    SocketType sock_type = SocketType::Dual;
#endif
    const char *bind_addr = nullptr;
    int port = 8888;
    const char *unix_path = nullptr;

    http_AddressMode addr_mode = http_AddressMode::Socket;

    int idle_timeout = 10000;
    int keepalive_time = 20000;
    int send_timeout = 60000;
    int stop_timeout = 10000;

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
    K_DELETE_COPY(http_Daemon)

    Size workers = 0;
    HeapArray<int> listeners;

#if defined(_WIN32)
    void *iocp = nullptr; // HANDLE
#endif

    SocketType sock_type;
    http_AddressMode addr_mode;

    int idle_timeout;
    int keepalive_time;
    int send_timeout;
    int stop_timeout;

    Size max_request_size;
    Size max_url_len;
    int max_request_headers;
    int max_request_cookies;

    Async *async = nullptr;
    http_Dispatcher *dispatcher = nullptr;

    std::function<void(http_IO *io)> handle_func;

public:
    http_Daemon() {}
    ~http_Daemon() { Stop(); }

    bool Bind(const http_Config &config, bool log_addr = true);
    bool Start(std::function<void(http_IO *io)> func);
    void Stop();

private:
    void StartRead(http_Socket *socket);
    void StartWrite(http_Socket *socket);
    void EndWrite(http_Socket *socket);

    Size ReadSocket(http_Socket *socket, Span<uint8_t> buf);
    bool WriteSocket(http_Socket *socket, Span<const uint8_t> buf);
    bool WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> bufs);

    bool InitConfig(const http_Config &config);
    void RunHandler(http_IO *client, int64_t now);

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

    http_KeyValue *next;
};

struct http_KeyHead {
    const char *key;

    http_KeyValue *first;
    http_KeyValue *last;

    K_HASHTABLE_HANDLER(http_KeyHead, key);
};

struct http_RequestInfo {
    int version = 10;
    bool keepalive = false;
    http_RequestMethod method = http_RequestMethod::Get;
    bool headers_only = false;
    const char *client_addr = nullptr;
    const char *path = nullptr;
    int64_t body_len = 0;

    HeapArray<http_KeyValue> values;
    HeapArray<http_KeyValue> headers;
    HeapArray<http_KeyValue> cookies;

private:
    HashTable<const char *, http_KeyHead> values_map;
    HashTable<const char *, http_KeyHead> headers_map;
    HashTable<const char *, http_KeyHead> cookies_map;

public:
    const http_KeyHead *FindQuery(const char *key) const;
    const http_KeyHead *FindHeader(const char *key) const;
    const http_KeyHead *FindCookie(const char *key) const;

    const char *GetQueryValue(const char *key) const;
    const char *GetHeaderValue(const char *key) const;
    const char *GetCookieValue(const char *key) const;

    friend class http_IO;
};

enum class http_RequestStatus {
    Busy,
    Ready,
    Close
};

enum class http_CookieFlag {
    HttpOnly = 1 << 0,
    Secure = 1 << 1
};

class http_IO {
    K_DELETE_COPY(http_IO)

    http_Daemon *daemon;

    http_Socket *socket;
    char addr[65] = {};

    int64_t socket_start;
    std::atomic_int64_t timeout_at;

    struct {
        HeapArray<uint8_t> buf;
        Size pos = 0;
        Span<uint8_t> extra = {};

        int64_t read = 0;
        bool reading = false;
    } incoming;

    http_RequestInfo request;

    const char *last_err = nullptr;

    struct {
        HeapArray<http_KeyValue> headers;

        bool started = false;
        int64_t expected = 0;
        int64_t sent = 0;
    } response;

    BlockAllocator allocator { Kibibytes(8) };

public:
    http_IO(http_Daemon *daemon) : daemon(daemon) { Rearm(-1); }

    const http_RequestInfo &Request() const { return request; }
    K::BlockAllocator *Allocator() { return &allocator; }

    bool NegociateEncoding(CompressionType preferred, CompressionType *out_encoding);
    bool NegociateEncoding(CompressionType preferred1, CompressionType preferred2, CompressionType *out_encoding);

    bool OpenForRead(int64_t max_len, StreamReader *out_st);

    void AddHeader(Span<const char> key, Span<const char> value);
    void AddEncodingHeader(CompressionType encoding);
    void AddCookieHeader(const char *path, const char *name, const char *value, unsigned int flags);
    void AddCachingHeaders(int64_t max_age, const char *etag = nullptr);

    bool OpenForWrite(int status, CompressionType encoding, int64_t len, StreamWriter *out_st);
    bool OpenForWrite(int status, int64_t len, StreamWriter *out_st)
        { return OpenForWrite(status, CompressionType::None, len, out_st); }

    void Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(StreamWriter *)> func);
    void Send(int status, int64_t len, FunctionRef<bool(StreamWriter *)> func)
        { Send(status, CompressionType::None, len, func); }

    void SendEmpty(int status);
    void SendText(int status, Span<const char> text, const char *mimetype = "text/plain");
    void SendBinary(int status, Span<const uint8_t> data, const char *mimetype = nullptr);
    void SendAsset(int status, Span<const uint8_t> data, const char *mimetype = nullptr,
                   CompressionType src_encoding = CompressionType::None);
    void SendError(int status, const char *msg = nullptr);
    void SendFile(int status, const char *filename, const char *mimetype = nullptr);
    void SendFile(int status, int fd, int64_t len = -1);

    void ExtendTimeout(int timeout);

    bool HasResponded() const { return response.started; }
    const char *LastError() const { return last_err; }

private:
    bool Init(http_Socket *socket, int64_t start, struct sockaddr *sa);

    http_RequestStatus ParseRequest();
    Span<const char> PrepareResponse(int status, CompressionType encoding, int64_t len);

    Size ReadDirect(Span<uint8_t> buf);
    bool WriteDirect(Span<const uint8_t> buf);
    bool WriteChunked(Span<const uint8_t> buf);

    // Returns true if connection is Keep-Alive and still within limits
    bool Rearm(int64_t now);
    bool IsBusy() const;

    friend class http_Daemon;
    friend class http_Dispatcher;
};

}
