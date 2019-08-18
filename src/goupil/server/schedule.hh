// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "../../wrappers/http.hh"

namespace RG {

int ProduceScheduleResources(const http_RequestInfo &request, http_Response *out_response);
int ProduceScheduleMeetings(const http_RequestInfo &request, http_Response *out_response);
int ProduceScheduleEvents(const http_RequestInfo &request, http_Response *out_response);

}
