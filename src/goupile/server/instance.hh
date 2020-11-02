// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

extern const int SchemaVersion;

enum SyncMode {
    Offline,
    Mirror
};
static const char *const SyncModeNames[] = {
    "Offline",
    "Mirror"
};

class InstanceData {
public:
    const char *root_directory = nullptr;
    const char *temp_directory = nullptr;
    const char *database_filename = nullptr;

    sq_Database db;
    int schema_version = -1;

    struct {
        const char *app_name = nullptr;
        const char *app_key = nullptr;

        bool use_offline = false;
        int max_file_size = (int)Megabytes(8);
        SyncMode sync_mode = SyncMode::Offline;
        const char *demo_user = nullptr;

        // XXX: Restore http_Config designated initializers when MSVC ICE is fixed
        // https://developercommunity.visualstudio.com/content/problem/1238876/fatal-error-c1001-ice-with-ehsc.html
        http_Config http;
        int max_age = 900;
    } config;

    BlockAllocator str_alloc;

    bool Open(const char *directory);
    bool Validate();
    bool Migrate();

    void Close();

private:
    bool LoadDatabaseConfig();
    bool LoadIniConfig(StreamReader *st);
};

}
