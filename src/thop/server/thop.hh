// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../lib/libmicrohttpd/src/include/microhttpd.h"

#include "../../libdrd/libdrd.hh"
#include "../../libcc/json.hh"

struct Config;
struct StructureSet;
struct UserSet;
struct User;

struct ConnectionInfo {
    MHD_Connection *conn;

    const User *user;
    bool user_mismatch;
    HashMap<const char *, Span<const char>> post;
    CompressionType compression_type;

    MHD_PostProcessor *pp = nullptr;

    BlockAllocator temp_alloc;
};

extern Config thop_config;
extern bool thop_has_casemix;

extern StructureSet thop_structure_set;
extern UserSet thop_user_set;
