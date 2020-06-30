// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../../vendor/libmicrohttpd/src/include/microhttpd.h"

namespace RG {

struct http_Config {
    IPStack ip_stack = IPStack::Dual;
    int port = 8888;

    int threads = std::max(GetCoreCount(), 4);
    int async_threads = std::max(GetCoreCount() * 2, 8);

    const char *base_url = "/";
};

struct http_RequestInfo;
class http_IO;

class http_Daemon {
    RG_DELETE_COPY(http_Daemon)

    MHD_Daemon *daemon = nullptr;

    const char *base_url;
    std::function<void(const http_RequestInfo &request, http_IO *io)> handle_func;

    Async *async = nullptr;

public:
    http_Daemon() {}
    ~http_Daemon() { Stop(); }

    bool Start(const http_Config &config,
               std::function<void(const http_RequestInfo &request, http_IO *io)> func);
    void Stop();

private:
    static MHD_Result HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                                    const char *, const char *upload_data, size_t *upload_data_size,
                                    void **con_cls);
    static ssize_t HandleWrite(void *cls, uint64_t pos, char *buf, size_t max);
    void RunNextAsync(http_IO *io);

    static void RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                 MHD_RequestTerminationCode toe);
};

struct http_RequestInfo {
    MHD_Connection *conn;

    // Useful in some cases (such as for cookie scopes)
    const char *base_url;

    const char *method;
    const char *url;
    CompressionType compression_type;

    const char *GetHeaderValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_HEADER_KIND, key); }
    const char *GetQueryValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key); }
    const char *GetCookieValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, key); }
};

class http_IO {
    RG_DELETE_COPY(http_IO)

    enum class State {
        Sync,
        Idle,
        Async,
        Zombie
    };

    http_Daemon *daemon;
    http_RequestInfo request;

    int code = -1;
    MHD_Response *response;

    std::mutex mutex;
    State state = State::Sync;
    bool suspended = false;

    std::function<void()> async_func;

    std::condition_variable read_cv;
    Span<uint8_t> read_buf = {};
    Size read_len = 0;
    bool read_eof = false;

    int write_code;
    std::condition_variable write_cv;
    HeapArray<uint8_t> write_buf;
    Size write_offset = 0;
    bool write_eof = false;

    HeapArray<std::function<void()>> finalizers;

public:
    BlockAllocator allocator;

    http_IO();
    ~http_IO();

    void RunAsync(std::function<void()> func);

    void AddHeader(const char *key, const char *value);
    void AddEncodingHeader(CompressionType compression_type);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
    void AddCachingHeaders(int max_age, const char *etag = nullptr);

    void AttachResponse(int code, MHD_Response *new_response);
    void AttachText(int code, Span<const char> str, const char *mime_type = "text/plain");
    bool AttachBinary(int code, Span<const uint8_t> data, const char *mime_type,
                      CompressionType compression_type = CompressionType::None);
    void AttachError(int code, const char *details = GetLastLogError());

    // Blocking, do in async context
    bool OpenForRead(StreamReader *out_st);
    bool ReadPostValues(Allocator *alloc,
                        HashMap<const char *, const char *> *out_values);
    bool OpenForWrite(int code, StreamWriter *out_st);

    void AddFinalizer(const std::function<void()> &func);

private:
    Size Read(Span<uint8_t> out_buf);
    bool Write(Span<const uint8_t> buf);

    // Call with mutex locked
    void Suspend();
    void Resume();

    friend http_Daemon;
};

}
