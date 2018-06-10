// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libdrd/libdrd.hh"
#include "json.hh"

PUSH_NO_WARNINGS()
#include "../../lib/libmicrohttpd/src/include/microhttpd.h"
POP_NO_WARNINGS()

struct Response {
    int code;
    MHD_Response *response;
};

extern const mco_TableSet *drdw_table_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> drdw_constraints_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> drdw_index_to_constraints;
extern const mco_AuthorizationSet *drdw_authorization_set;
extern mco_StaySet drdw_stay_set;

const mco_TableIndex *GetIndexFromQueryString(MHD_Connection *conn, const char *redirect_url,
                                              Response *out_response);

Response CreateErrorPage(int code);
MHD_Response *BuildJson(CompressionType compression_type,
                        std::function<bool(rapidjson::Writer<JsonStreamWriter> &)> func);

Response ProduceIndexes(MHD_Connection *conn, const char *url, CompressionType compression_type);
Response ProduceClassifierTree(MHD_Connection *conn, const char *, CompressionType compression_type);
Response ProduceDiagnoses(MHD_Connection *conn, const char *url, CompressionType compression_type);
Response ProduceProcedures(MHD_Connection *conn, const char *url, CompressionType compression_type);
Response ProduceGhmGhs(MHD_Connection *conn, const char *url, CompressionType compression_type);

Response ProduceCaseMix(MHD_Connection *conn, const char *url, CompressionType compression_type);
