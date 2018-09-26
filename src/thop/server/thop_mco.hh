// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "thop.hh"

Response ProduceMcoCaseMix(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response ProduceMcoClassify(const ConnectionInfo *conn, const char *, CompressionType compression_type);

Response ProduceMcoIndexes(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response ProduceMcoDiagnoses(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response ProduceMcoProcedures(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response ProduceMcoGhmGhs(const ConnectionInfo *conn, const char *url, CompressionType compression_type);

Response ProduceMcoTree(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
