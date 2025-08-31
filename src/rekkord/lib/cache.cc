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

#include "src/core/base/base.hh"
#include "cache.hh"
#include "disk.hh"
#include "repository.hh"

namespace K {

static const int CacheVersion = 2;
static const int64_t CommitDelay = 5000;

bool rk_Cache::Open(rk_Repository *repo, bool build)
{
    K_ASSERT(!this->repo);
    K_ASSERT(!main.IsValid());
    K_ASSERT(!write.IsValid());

    BlockAllocator temp_alloc;

    K_DEFER_N(err_guard) { Close(); };

    this->repo = repo;

    const char *filename;
    {
        uint8_t id[32];
        repo->MakeID(id);

        const char *dirname = GetUserCachePath("rekkord", &temp_alloc);
        if (!dirname) {
            LogError("Cannot find user cache path");
            return false;
        }
        if (!MakeDirectory(dirname, false))
            return false;

        filename = Fmt(&temp_alloc, "%1%/%2.db", dirname, FmtHex(id, FmtType::SmallHex)).ptr;
        LogDebug("Cache file: %1", filename);
    }

    if (!main.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!main.SetWAL(true))
        return false;
    if (!main.Run("PRAGMA synchronous = NORMAL"))
        return false;

    int version;
    if (!main.GetUserVersion(&version))
        return false;

    if (version > CacheVersion) {
        LogError("Cache schema is too recent (%1, expected %2)", version, CacheVersion);
        return false;
    } else if (version < CacheVersion) {
        bool success = main.Transaction([&]() {
            switch (version) {
                case 0: {
                    bool success = main.RunMany(R"(
                        CREATE TABLE meta (
                            cid BLOB
                        );

                        CREATE TABLE stats (
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            ctime INTEGER NOT NULL,
                            mode INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            hash BLOB NOT NULL,
                            stored INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX stats_p ON stats (path);

                        CREATE TABLE blobs (
                            oid BLOB NOT NULL,
                            size INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX blobs_k ON blobs (oid);

                        CREATE TABLE checks (
                            oid BLOB NOT NULL,
                            mark INTEGER NOT NULL,
                            valid INTEGER CHECK (valid IN (0, 1)) NOT NULL
                        );
                        CREATE UNIQUE INDEX checks_o ON checks (oid);
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 1: {
                    bool success = main.RunMany(R"(
                        ALTER TABLE meta RENAME TO meta_BAK;

                        CREATE TABLE meta (
                            cid BLOB,
                            exhaustive INTEGER CHECK (exhaustive IN (0, 1)) NOT NULL
                        );

                        INSERT INTO meta (cid, exhaustive)
                            SELECT cid, 1 FROM meta_BAK;

                        DROP TABLE meta_BAK;
                    )");
                    if (!success)
                        return false;
                } // [[fallthrough]];

                static_assert(CacheVersion == 2);
            }

            if (!main.SetUserVersion(CacheVersion))
                return false;

            return true;
        });

        if (!success)
            return false;
    }

    bool reset = false;

    // Check known CID against repository CID
    {
        sq_Statement stmt;
        if (!main.Prepare("SELECT cid, exhaustive FROM meta", &stmt))
            return false;

        if (stmt.Step()) {
            Span<const uint8_t> cid1 = repo->GetCID();
            Span<const uint8_t> cid2 = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 0),
                                                sqlite3_column_bytes(stmt, 0));
            bool exhaustive = sqlite3_column_int(stmt, 1);

            if (cid1 != cid2) {
                reset = true;
            } else if (build && !exhaustive) {
                reset = true;
            }
        } else if (stmt.IsValid()) {
            if (!main.Run("INSERT INTO meta (cid, exhaustive) VALUES (NULL, 0)"))
                return false;

            reset = true;
        } else {
            return false;
        }
    }

    if (reset) {
        if (build) {
            LogInfo("Rebuilding cache...");
        } else {
            LogInfo("Resetting cache...");
        }

        bool success = main.Transaction([&]() { return Reset(build); });
        if (!success)
            return false;
    }

    if (!write.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;

    err_guard.Disable();
    return true;
}

bool rk_Cache::Close()
{
    if (!repo)
        return true;

    // Can't use lock_guard, see Commit()
    put_mutex.lock();

    bool success = Commit(true);

    main.Close();
    write.Close();
    repo = nullptr;

    return success;
}

bool rk_Cache::Reset(bool list)
{
    K_ASSERT(repo);

    bool success = main.Transaction([&]() {
        if (!main.Run("DELETE FROM stats"))
            return false;
        if (!main.Run("DELETE FROM blobs"))
            return false;

        if (list) {
            rk_Disk *disk = repo->GetDisk();

            ProgressHandle progress("Cache");
            std::atomic_int64_t listed { 0 };

            bool success = disk->ListFiles("blobs", [&](const char *path, int64_t size) {
                // We never write empty blobs, something went wrong
                if (!size) [[unlikely]]
                    return true;
                if (!StartsWith(path, "blobs/")) [[unlikely]]
                    return true;

                rk_ObjectID oid;
                {
                    Span<const char> remain = path + 6;
                    Span<const char> catalog = SplitStr(remain, '/');
                    Span<const char> hash = SplitStrReverse(remain, '/');

                    if (catalog.len != 1)
                        return true;

                    char str[128];
                    str[0] = catalog[0];
                    CopyString(hash, MakeSpan(str + 1, K_SIZE(str) - 1));

                    if (!rk_ParseOID(str, &oid))
                        return true;
                }

                if (!main.Run(R"(INSERT INTO blobs (oid, size)
                                 VALUES (?1, ?2)
                                 ON CONFLICT (oid) DO UPDATE SET size = excluded.size)",
                              oid.Raw(), size))
                    return false;

                int64_t blobs = listed.fetch_add(1, std::memory_order_relaxed) + 1;
                progress.SetFmt(T("%1 cached"), blobs);

                return true;
            });
            if (!success)
                return false;
        }

        if (!main.Run("UPDATE meta SET cid = ?1, exhaustive = ?2", repo->GetCID(), 0 + list))
            return false;

        return true;
    });

    return success;
}

int64_t rk_Cache::CountBlobs(int64_t *out_checked)
{
    sq_Statement stmt;
    if (!main.Prepare(R"(SELECT COUNT(b.oid) AS blobs,
                                SUM(IIF(c.oid IS NULL, 0, 1)) AS checked
                         FROM blobs b
                         LEFT JOIN checks c ON (c.oid = b.oid))", &stmt))
        return -1;
    if (!stmt.Step()) {
        K_ASSERT(!stmt.IsValid());
        return -1;
    }

    int64_t blobs = sqlite3_column_int64(stmt, 0);
    int64_t checked = sqlite3_column_int64(stmt, 1);

    if (out_checked) {
        *out_checked = checked;
    }
    return blobs;
}

bool rk_Cache::PruneChecks(int64_t from)
{
    K_ASSERT(repo);

    bool success = main.Run("DELETE FROM checks WHERE mark < ?1", from);
    return success;
}

StatResult rk_Cache::TestBlob(const rk_ObjectID &oid, int64_t *out_size)
{
    K_ASSERT(repo);

    sq_Statement stmt;
    if (!main.Prepare("SELECT size FROM blobs WHERE oid = ?1", &stmt, oid.Raw()))
        return StatResult::OtherError;

    if (stmt.Step()) {
        int64_t size = sqlite3_column_int64(stmt, 0);

        if (out_size) {
            *out_size = size;
        }
        return StatResult::Success;
    } else if (stmt.IsValid()) {
        return StatResult::MissingPath;
    } else {
        return StatResult::OtherError;
    }
}

bool rk_Cache::HasCheck(const rk_ObjectID &oid, bool *out_valid)
{
    K_ASSERT(repo);

    sq_Statement stmt;
    if (!main.Prepare("SELECT valid FROM checks WHERE oid = ?1", &stmt, oid.Raw()))
        return false;
    if (!stmt.Step())
        return false;

    bool valid = sqlite3_column_int(stmt, 0);

    if (out_valid) {
        *out_valid = valid;
    }
    return true;
}

StatResult rk_Cache::GetStat(const char *path, rk_CacheStat *out_stat)
{
    K_ASSERT(repo);

    sq_Statement stmt;
    if (!main.Prepare(R"(SELECT mtime, ctime, mode, size, hash, stored
                         FROM stats
                         WHERE path = ?1)", &stmt, path))
        return StatResult::OtherError;

    if (stmt.Step()) {
        Span<const uint8_t> hash = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 4),
                                                             sqlite3_column_bytes(stmt, 4));

        if (hash.len != K_SIZE(out_stat->hash)) [[unlikely]] {
            LogDebug("Hash size mismatch for '%1'", path);
            return StatResult::MissingPath;
        }

        out_stat->mtime = sqlite3_column_int64(stmt, 0);
        out_stat->ctime = sqlite3_column_int64(stmt, 1);
        out_stat->mode = (uint32_t)sqlite3_column_int64(stmt, 2);
        out_stat->size = sqlite3_column_int64(stmt, 3);
        MemCpy(out_stat->hash.raw, hash.ptr, hash.len);
        out_stat->stored = sqlite3_column_int64(stmt, 5);

        return StatResult::Success;
    } else if (stmt.IsValid()) {
        return StatResult::MissingPath;
    } else {
        return StatResult::OtherError;
    }
}

void rk_Cache::PutBlob(const rk_ObjectID &oid, int64_t size)
{
    K_ASSERT(repo);

    // Can't use lock_guard, see Commit()
    put_mutex.lock();

    PendingBlob blob = { oid, size };
    pending.blobs.Append(blob);

    Commit(false);
}

bool rk_Cache::PutCheck(const rk_ObjectID &oid, int64_t mark, bool valid)
{
    K_ASSERT(repo);

    // Can't use lock_guard, see Commit()
    put_mutex.lock();

    PendingCheck check = { oid, mark, valid };
    pending.checks.Append(check);

    bool inserted;
    known_checks.TrySet(oid, &inserted);

    Commit(false);

    if (!inserted) {
        // The check may have been committed already
        inserted = !HasCheck(oid);
    }

    return inserted;
}

void rk_Cache::PutStat(const char *path, const rk_CacheStat &st)
{
    K_ASSERT(repo);

    // Can't use lock_guard, see Commit()
    put_mutex.lock();

    path = DuplicateString(path, &pending.str_alloc).ptr;

    PendingStat stat = { path, st };
    pending.stats.Append(stat);

    Commit(false);
}

// Call with put_mutex locked, but don't use a guard because this method releases the lock!
bool rk_Cache::Commit(bool force)
{
    K_ASSERT(repo);

    int64_t now = GetMonotonicTime();

    if (!force && now - last_commit < CommitDelay) {
        put_mutex.unlock();
        return true;
    }

    std::unique_lock<std::mutex> lock_commit(commit_mutex);

    std::swap(pending, commit);
    last_commit = now;

    put_mutex.unlock();

    K_DEFER {
        commit.blobs.RemoveFrom(0);
        commit.checks.RemoveFrom(0);
        commit.stats.RemoveFrom(0);
        commit.str_alloc.Reset();

        lock_commit.unlock();

        std::lock_guard<std::mutex> lock_put(put_mutex);

        known_checks.RemoveAll();
        for (const PendingCheck &check: pending.checks) {
            known_checks.Set(check.oid);
        }
    };

    bool success = write.Transaction([&]() {
        for (const PendingBlob &blob: commit.blobs) {
            if (!write.Run(R"(INSERT INTO blobs (oid, size)
                              VALUES (?1, ?2)
                              ON CONFLICT DO NOTHING)",
                           blob.oid.Raw(), blob.size))
                return false;
        }

        for (const PendingCheck &check: commit.checks) {
            if (!write.Run(R"(INSERT INTO checks (oid, mark, valid)
                              VALUES (?1, ?2, ?3)
                              ON CONFLICT (oid) DO UPDATE SET mark = excluded.mark,
                                                              valid = excluded.valid)",
                           check.oid.Raw(), check.mark, 0 + check.valid))
                return false;
        }

        for (const PendingStat &stat: commit.stats) {
            Span<const uint8_t> hash = MakeSpan((const uint8_t *)&stat.st.hash.raw, K_SIZE(stat.st.hash.raw));

            if (!write.Run(R"(INSERT INTO stats (path, mtime, ctime, mode, size, hash, stored)
                              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
                              ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                               ctime = excluded.ctime,
                                                               mode = excluded.mode,
                                                               size = excluded.size,
                                                               hash = excluded.hash,
                                                               stored = excluded.stored)",
                           stat.path, stat.st.mtime, stat.st.ctime, stat.st.mode, stat.st.size, hash, stat.st.stored))
                return false;
        }

        return true;
    });

    return success;
}

}
