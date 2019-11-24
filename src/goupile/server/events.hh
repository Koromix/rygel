// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

enum class EventType {
    Schedule,
    KeepAlive
};
static const char *const EventTypeNames[] = {
    "schedule",
    "keepalive"
};

void CloseAllEventConnections();

void HandleEvents(const http_RequestInfo &request, http_IO *io);

void PushEvent(EventType type);

}
