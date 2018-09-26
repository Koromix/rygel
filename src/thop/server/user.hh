// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "thop.hh"

const User *CheckSessionUser(MHD_Connection *conn);

Response ProduceSession(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response HandleConnect(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
Response HandleDisconnect(const ConnectionInfo *conn, const char *url, CompressionType compression_type);
