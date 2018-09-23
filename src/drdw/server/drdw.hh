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

    MHD_PostProcessor *pp = nullptr;
    LinkedAllocator temp_alloc;
};

struct Response {
    enum class Flag {
        DisableCacheControl = 1 << 0,
        DisableETag = 1 << 1
    };

    int code;
    MHD_Response *response;
    unsigned int flags;
};

extern const mco_TableSet *drdw_table_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> drdw_constraints_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> drdw_index_to_constraints;

extern const mco_AuthorizationSet *drdw_authorization_set;
extern UserSet drdw_user_set;
extern StructureSet drdw_structure_set;
extern mco_StaySet drdw_stay_set;
extern Date drdw_stay_set_dates[2];

Response CreateErrorPage(int code);
MHD_Response *BuildJson(CompressionType compression_type,
                        std::function<bool(rapidjson::Writer<JsonStreamWriter> &)> func);

const User *CheckSessionUser(MHD_Connection *conn);
Response HandleConnect(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response HandleDisconnect(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response ProduceSession(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
