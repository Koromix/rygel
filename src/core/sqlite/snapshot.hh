// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

namespace RG {

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
