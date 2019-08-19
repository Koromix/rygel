// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "user.hh"
#include "../../wrappers/http.hh"

namespace RG {

int ProduceMcoDiagnoses(const http_RequestInfo &request, const User *user, http_IO *io);
int ProduceMcoProcedures(const http_RequestInfo &request, const User *user, http_IO *io);
int ProduceMcoGhmGhs(const http_RequestInfo &request, const User *user, http_IO *io);

int ProduceMcoTree(const http_RequestInfo &request, const User *user, http_IO *io);

}
