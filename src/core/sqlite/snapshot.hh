// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"

namespace K {

struct sq_SnapshotGeneration {
    const char *base_filename;
    Size frame_idx;
    Size frames;
    int64_t ctime;
    int64_t mtime;
};

struct sq_SnapshotFrame {
    int64_t mtime;
    Size generation_idx;
    uint8_t sha256[32];
};

struct sq_SnapshotInfo {
    const char *orig_filename;

    int64_t ctime;
    int64_t mtime;
    HeapArray<sq_SnapshotGeneration> generations;
    HeapArray<sq_SnapshotFrame> frames;

    Size FindFrame(int64_t mtime) const;
};

struct sq_SnapshotSet {
    HeapArray<sq_SnapshotInfo> snapshots;

    BlockAllocator str_alloc;
};

bool sq_CollectSnapshots(Span<const char *> filenames, sq_SnapshotSet *out_set);
bool sq_RestoreSnapshot(const sq_SnapshotInfo &snapshot, Size frame_idx, const char *dest_filename, bool overwrite);

}
