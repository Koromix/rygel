// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
#include "../libcc/libcc.hh"
#include "json.hh"

struct http_Response {
    enum class Flag {
        DisableCache = 1 << 0,
        DisableETag = 1 << 1
    };

    struct ResponseDeleter {
        void operator()(MHD_Response *response) { MHD_destroy_response(response); }
    };

    std::unique_ptr<MHD_Response, ResponseDeleter> response;
    unsigned int flags = 0;

    http_Response &operator=(MHD_Response *response) {
        this->response.reset(response);
        return *this;
    }
    operator MHD_Response *() const { return response.get(); }

    void AddEncodingHeader(CompressionType compression_type);
    void AddCookieHeader(const char *path, const char *name, const char *value,
                         bool http_only = false);
};

struct http_Connection {
    MHD_Connection *conn;

    HashMap<const char *, Span<const char>> post;
    CompressionType compression_type;

    MHD_PostProcessor *pp = nullptr;

    BlockAllocator temp_alloc;
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
