// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "thop.hh"

int ProduceMcoSettings(const ConnectionInfo *conn, const char *url, Response *out_response);

int ProduceMcoCasemixUnits(const ConnectionInfo *conn, const char *, Response *out_response);
int ProduceMcoCasemixDuration(const ConnectionInfo *conn, const char *, Response *out_response);

int ProduceMcoResults(const ConnectionInfo *conn, const char *, Response *out_response);
