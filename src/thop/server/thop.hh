// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../lib/libmicrohttpd/src/include/microhttpd.h"

#include "../../libdrd/libdrd.hh"
#include "config.hh"
#include "json.hh"

struct ConnectionInfo {
    MHD_Connection *conn;

    const User *user;
    HashMap<const char *, Span<const char>> post;
    CompressionType compression_type;

    MHD_PostProcessor *pp = nullptr;

    BlockAllocator temp_alloc {Kibibytes(16)};
};

struct Response {
    enum class Flag {
        DisableCacheControl = 1 << 0,
        DisableETag = 1 << 1
    };

    struct ResponseDeleter {
        void operator()(MHD_Response *response) { MHD_destroy_response(response); }
    };

    std::unique_ptr<MHD_Response, ResponseDeleter> response;
    unsigned int flags = 0;
};

extern const mco_TableSet *thop_table_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> thop_constraints_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> thop_index_to_constraints;

extern const mco_AuthorizationSet *thop_authorization_set;
extern UserSet thop_user_set;
extern StructureSet thop_structure_set;
extern mco_StaySet thop_stay_set;
extern Date thop_stay_set_dates[2];

void AddContentEncodingHeader(MHD_Response *response, CompressionType compression_type);
void AddCookieHeader(MHD_Response *response, const char *name, const char *value,
                     bool http_only = false);

int CreateErrorPage(int code, Response *out_response);

int BuildJson(std::function<bool(rapidjson::Writer<JsonStreamWriter> &)> func,
              CompressionType compression_type, Response *out_response);
