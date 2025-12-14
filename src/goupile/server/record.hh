// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"

namespace K {

class InstanceHolder;

struct ExportSettings {
    int64_t thread = -1;
    int64_t anchor = -1;
    bool scheduled = false;
};

struct ExportInfo {
    int64_t max_thread;
    int64_t max_anchor;
    char secret[33];
};

void HandleRecordList(http_IO *io, InstanceHolder *instance);
void HandleRecordGet(http_IO *io, InstanceHolder *instance);
void HandleRecordAudit(http_IO *io, InstanceHolder *instance);

void HandleRecordReserve(http_IO *io, InstanceHolder *instance);
void HandleRecordSave(http_IO *io, InstanceHolder *instance);
void HandleRecordDelete(http_IO *io, InstanceHolder *instance);
void HandleRecordLock(http_IO *io, InstanceHolder *instance);
void HandleRecordUnlock(http_IO *io, InstanceHolder *instance);
void HandleRecordPublic(http_IO *io, InstanceHolder *instance);

int64_t ExportRecords(InstanceHolder *instance, int64_t userid, const ExportSettings &settings, ExportInfo *out_info = nullptr);
bool PerformScheduledExport(InstanceHolder *instance);

void HandleBulkExport(http_IO *io, InstanceHolder *instance);
void HandleBulkList(http_IO *io, InstanceHolder *instance);
void HandleBulkDownload(http_IO *io, InstanceHolder *instance);
void HandleBulkImport(http_IO *io, InstanceHolder *instance);

void HandleBlobGet(http_IO *io, InstanceHolder *instance);
void HandleBlobPost(http_IO *io, InstanceHolder *instance);

}
