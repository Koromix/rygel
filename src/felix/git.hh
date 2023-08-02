// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/libcc/libcc.hh"

namespace RG {

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
    HashMap<GitHash, const char *> hash_map;
    HashSet<Span<const char>> prefix_set;
    HeapArray<const char *> idx_filenames;
    HeapArray<const char *> pack_filenames;

    // Already known commit IDs
    HeapArray<GitHash> commits;

    // Reuse for performance
    HeapArray<char> loose_filename;

    BlockAllocator str_alloc;

public:
    bool Prepare(const char *root_directory);

    bool IsValid() const { return commits.len; }

    // String remains valid until object is destroyed
    const char *Version(Span<const char> key);

private:
    bool ReadAttribute(Span<const char> id, const char *attr, GitHash *out_hash);
    bool ReadAttribute(const GitHash &hash, const char *attr, GitHash *out_hash);

    bool ReadLooseAttribute(const char *filename, const char *attr, GitHash *out_hash);

    bool FindInIndexes(Size start_idx, const GitHash &hash, PackLocation *out_location);
    bool ReadPackAttribute(Size idx, Size offset, const char *attr, GitHash *out_hash);
    bool ReadPackObject(FILE *fp, Size offset, int *out_type, HeapArray<uint8_t> *out_obj);
};

}
