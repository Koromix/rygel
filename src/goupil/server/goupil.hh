// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "config.hh"
#include "data.hh"

namespace RG {

enum class EventType {
    Schedule,
    KeepAlive
};
static const char *const EventTypeNames[] = {
    "schedule",
    "keepalive"
};

void PushEvent(EventType type);

extern Config goupil_config;
extern SQLiteDatabase goupil_db;

}
