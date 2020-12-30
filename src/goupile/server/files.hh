// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

class InstanceHolder;

void HandleFileList(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
bool HandleFileGet(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleFilePut(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleFileDelete(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

}
