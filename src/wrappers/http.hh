// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../vendor/libmicrohttpd/src/include/microhttpd.h"
#include "../libcc/libcc.hh"
#include "json.hh"

namespace RG {

class http_Daemon;

struct http_RequestInfo {
    MHD_Connection *conn;

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
    enum class State {
        First,
        Read,
        Async,
        Done
    };

    http_RequestInfo request;

    int code = -1;
    MHD_Response *response;

    std::mutex mutex;
    State state = State::First;
    bool suspended = false;

    std::function<void(const http_RequestInfo &request, http_IO *io)> async_func;

    std::condition_variable read_cv;
    HeapArray<uint8_t> read_buf;

public:
    enum class Flag {
        EnableCacheControl = 1 << 0,
        EnableETag = 1 << 1,
        EnableCache = (int)EnableCacheControl | (int)EnableETag
    };

    BlockAllocator allocator;

    unsigned int flags = 0;

    http_IO();
    ~http_IO();

    void RunAsync(std::function<void(const http_RequestInfo &request, http_IO *io)> func);

    void AddHeader(const char *key, const char *value);
    void AddEncodingHeader(CompressionType compression_type);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
    void AddCachingHeaders(int max_age, const char *etag = nullptr);

    void AttachResponse(int code, MHD_Response *new_response);

    // Blocking, do in async context
    bool ReadPostValues(Allocator *alloc,
                        HashMap<const char *, const char *> *out_values);

private:
    // Call with mutex locked
    void Suspend();
    void Resume();

    friend http_Daemon;
};

class http_Daemon {
    MHD_Daemon *daemon = nullptr;
    const char *base_url;

    Async *async = nullptr;

public:
    ~http_Daemon() { Stop(); }

    std::function<void(const http_RequestInfo &request, http_IO *io)> handle_func;
    std::function<void(const http_RequestInfo &request, MHD_RequestTerminationCode code)> release_func;

    bool Start(IPStack stack, int port, int threads, const char *base_url);
    void Stop();

private:
    static int HandleRequest(void *cls, MHD_Connection *conn, const char *url, const char *method,
                             const char *, const char *upload_data, size_t *upload_data_size,
                             void **con_cls);
    static void RequestCompleted(void *cls, MHD_Connection *, void **con_cls,
                                 MHD_RequestTerminationCode toe);
};

const char *http_GetMimeType(Span<const char> extension);

uint32_t http_ParseAcceptableEncodings(Span<const char> encodings);

void http_ProduceErrorPage(int code, http_IO *io);
bool http_ProduceStaticAsset(Span<const uint8_t> data, CompressionType in_compression_type,
                             const char *mime_type, CompressionType out_compression_type,
                             http_IO *io);

class http_JsonPageBuilder: public json_Writer {
    HeapArray<uint8_t> buf;
    StreamWriter st;

public:
    http_JsonPageBuilder(CompressionType compression_type) :
        json_Writer(&st), st(&buf, nullptr, compression_type) {}

    void Finish(http_IO *io);
};

}
