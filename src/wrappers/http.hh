// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../vendor/libmicrohttpd/src/include/microhttpd.h"
#include "../libcc/libcc.hh"
#include "json.hh"

namespace RG {

class http_Response {
    struct ResponseDeleter {
        void operator()(MHD_Response *response) { MHD_destroy_response(response); }
    };

    std::unique_ptr<MHD_Response, ResponseDeleter> response;

public:
    enum class Flag {
        EnableCacheControl = 1 << 0,
        EnableETag = 1 << 1,
        EnableCache = (int)EnableCacheControl | (int)EnableETag
    };

    unsigned int flags = 0;

    http_Response();

    operator MHD_Response *() const { return response.get(); }

    void AddEncodingHeader(CompressionType compression_type);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
    void AddCachingHeaders(int max_age, const char *etag = nullptr);

    void AttachResponse(MHD_Response *new_response);

    friend class http_Daemon;
};

class http_RequestInfo {
    MHD_PostProcessor *pp;
    BlockAllocator alloc;

public:
    MHD_Connection *conn;

    const char *method;
    const char *url;
    CompressionType compression_type;
    HashMap<const char *, Span<const char>> post;

    const char *GetHeaderValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_HEADER_KIND, key); }
    const char *GetQueryValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, key); }
    const char *GetCookieValue(const char *key) const
        { return MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, key); }
    const char *GetPostValue(const char *key) const
        { return post.FindValue(key, {}).ptr; }

    friend class http_Daemon;
};

class http_Daemon {
    MHD_Daemon *daemon = nullptr;
    const char *base_url;

public:
    ~http_Daemon() { Stop(); }

    std::function<int(const http_RequestInfo &request, http_Response *out_response)> handle_func;
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

int http_ProduceErrorPage(int code, http_Response *out_response);
int http_ProduceStaticAsset(Span<const uint8_t> data, CompressionType in_compression_type,
                            const char *mime_type, CompressionType out_compression_type,
                            http_Response *out_response);

class http_JsonPageBuilder: public json_Writer {
    HeapArray<uint8_t> buf;
    StreamWriter st;

public:
    http_JsonPageBuilder(CompressionType compression_type) :
        json_Writer(&st), st(&buf, nullptr, compression_type) {}

    int Finish(http_Response *out_response);
};

}
