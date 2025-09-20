// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"

namespace K {

union GitHash {
    uint8_t raw[20] = {};
    uint32_t u[5];
    uint64_t u64;

    uint64_t Hash() const { return u64; }

    bool operator==(const GitHash &other) const { return u[0] == other.u[0] && u[1] == other.u[1] && u[2] == other.u[2] && u[3] == other.u[3] && u[4] == other.u[4]; }
    bool operator!=(const GitHash &other) const { return u[0] != other.u[0] || u[1] != other.u[1] || u[2] != other.u[2] || u[3] != other.u[3] || u[4] != other.u[4]; }
};

class GitVersioneer {
    struct PackLocation {
        Size idx;
        Size offset;
    };

    const char *repo_directory = nullptr;

    // Prepared stuff
    HashMap<const char *, GitHash> ref_map;
    HashMap<GitHash, HeapArray<const char *>> hash_map;
    HashMap<Span<const char>, int64_t> prefix_map;
    HeapArray<const char *> idx_filenames;
    HeapArray<const char *> pack_filenames;

    // Descriptor cache
    HeapArray<int> idx_files;
    HeapArray<int> pack_files;

    // Already known commit IDs
    HeapArray<GitHash> commits;

    // Reuse for performance
    HeapArray<Span<char>> loose_filenames;

    BlockAllocator str_alloc;

public:
    int64_t max_delta_date = 14 * 86400000ll;
    Size max_delta_count = 2000;

    ~GitVersioneer();

    static bool IsAvailable();

    bool Prepare(const char *root_directory);

    bool IsValid() const { return commits.len; }

    // String remains valid until object is destroyed
    const char *Version(Span<const char> key, Allocator *alloc);

private:
    bool CacheTagInfo(Span<const char> tag, Span<const char> id);

    bool ReadAttributes(Span<const char> id, FunctionRef<bool(Span<const char> key, Span<const char> value)> func);
    bool ReadAttributes(const GitHash &hash, FunctionRef<bool(Span<const char> key, Span<const char> value)> func);

    bool ReadLooseAttributes(const char *filename, FunctionRef<bool(Span<const char> key, Span<const char> value)> func);

    bool FindInIndexes(Size start_idx, const GitHash &hash, PackLocation *out_location);
    bool ReadPackAttributes(Size idx, int64_t offset, FunctionRef<bool(Span<const char> key, Span<const char> value)> func);
    bool ReadPackObject(int fd, int64_t offset, int *out_type, HeapArray<uint8_t> *out_obj);
};

}
