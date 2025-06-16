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
#include "src/core/sqlite/sqlite.hh"
#include "repository.hh"

namespace RG {

struct rk_CacheStat {
    int64_t ctime;
    int64_t mtime;
    uint32_t mode;
    int64_t size;

    rk_Hash hash;
    int64_t stored;
};

class rk_Cache {
    struct PendingBlob {
        rk_ObjectID oid;
        int64_t size;
    };

    struct PendingCheck {
        rk_ObjectID oid;
        int64_t mark;
        bool valid;
    };

    struct PendingStat {
        const char *path;
        rk_CacheStat st;
    };

    struct PendingSet {
        HeapArray<PendingBlob> blobs;
        HeapArray<PendingCheck> checks;
        HeapArray<PendingStat> stats;

        BlockAllocator str_alloc;
    };

    rk_Repository *repo = nullptr;

    sq_Database main;
    sq_Database write;

    std::mutex put_mutex;
    PendingSet pending;
    int64_t last_commit = 0;
    std::mutex commit_mutex;
    PendingSet commit;
    HashSet<rk_ObjectID> known_checks;

public:
    ~rk_Cache() { Close(); }

    bool Open(rk_Repository *repo, bool build);
    bool Close();

    bool Reset(bool list);

    int64_t CountBlobs(int64_t *out_checked = nullptr);
    bool PruneChecks(int64_t from);

    StatResult TestBlob(const rk_ObjectID &oid, int64_t *out_size = nullptr);
    bool HasCheck(const rk_ObjectID &oid, bool *out_valid = nullptr);
    StatResult GetStat(const char *path, rk_CacheStat *st);

    void PutBlob(const rk_ObjectID &oid, int64_t size);
    bool PutCheck(const rk_ObjectID &oid, int64_t mark, bool valid);
    void PutStat(const char *path, const rk_CacheStat &stat);

private:
    bool Commit(bool force);
};

}
