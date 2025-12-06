// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/sqlite/sqlite.hh"
#include "repository.hh"

namespace K {

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

public:
    ~rk_Cache() { Close(); }

    bool Open(rk_Repository *repo, bool build);
    bool Close();

    bool Reset(bool list);

    bool PruneChecks(int64_t from);
    int64_t CountChecks();
    bool ListChecks(FunctionRef<bool(const rk_ObjectID &)> func);

    StatResult TestBlob(const rk_ObjectID &oid, int64_t *out_size = nullptr);
    bool HasCheck(const rk_ObjectID &oid, bool *out_valid = nullptr);
    StatResult GetStat(const char *path, rk_CacheStat *st);

    void PutBlob(const rk_ObjectID &oid, int64_t size);
    void PutCheck(const rk_ObjectID &oid, int64_t mark, bool valid);
    void PutStat(const char *path, const rk_CacheStat &stat);

private:
    bool Commit(bool force);
};

}
