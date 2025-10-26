// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace K {

class InstanceHolder;

struct ExportSettings {
    int64_t sequence = -1;
    int64_t anchor = -1;
    bool scheduled = false;
};

struct ExportInfo {
    int64_t max_sequence;
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

void HandleExportCreate(http_IO *io, InstanceHolder *instance);
void HandleExportList(http_IO *io, InstanceHolder *instance);
void HandleExportDownload(http_IO *io, InstanceHolder *instance);

void HandleBlobGet(http_IO *io, InstanceHolder *instance);
void HandleBlobPost(http_IO *io, InstanceHolder *instance);

}
