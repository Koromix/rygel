// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "thop.hh"

struct Response {
    enum class Flag {
        DisableCache = 1 << 0,
        DisableETag = 1 << 1
    };

    struct ResponseDeleter {
        void operator()(MHD_Response *response) { MHD_destroy_response(response); }
    };

    std::unique_ptr<MHD_Response, ResponseDeleter> response;
    unsigned int flags = 0;

    Response &operator=(MHD_Response *response) {
        this->response.reset(response);
        return *this;
    }
    operator MHD_Response *() const { return response.get(); }
};

void AddContentEncodingHeader(MHD_Response *response, CompressionType compression_type);
void AddCookieHeader(MHD_Response *response, const char *name, const char *value,
                     bool http_only = false);

int CreateErrorPage(int code, Response *out_response);

class JsonPageBuilder: public JsonWriter {
    HeapArray<uint8_t> buf;
    StreamWriter st;

public:
    JsonPageBuilder(CompressionType compression_type) :
        JsonWriter(&st), st(&buf, nullptr, compression_type) {}

    int Finish(Response *out_response);
};
