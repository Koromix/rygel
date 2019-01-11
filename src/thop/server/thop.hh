// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../lib/libmicrohttpd/src/include/microhttpd.h"

#include "../../libdrd/libdrd.hh"
#include "../../libcc/json.hh"

struct Config;
struct StructureSet;
struct UserSet;
struct User;

struct ConnectionInfo {
    MHD_Connection *conn;

    const User *user;
    bool user_mismatch;
    HashMap<const char *, Span<const char>> post;
    CompressionType compression_type;

    MHD_PostProcessor *pp = nullptr;

    BlockAllocator temp_alloc;
};

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

extern Config thop_config;
extern bool thop_has_casemix;

extern StructureSet thop_structure_set;
extern UserSet thop_user_set;

void AddContentEncodingHeader(MHD_Response *response, CompressionType compression_type);
void AddCookieHeader(MHD_Response *response, const char *name, const char *value,
                     bool http_only = false);

int CreateErrorPage(int code, Response *out_response);

int BuildJson(std::function<bool(JsonWriter &)> func,
              CompressionType compression_type, Response *out_response);
