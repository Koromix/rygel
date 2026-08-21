// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/wrap/sqlite.hh"
#include "web.hh"
#include "user.hh"
#include "utility.hh"
#include "words.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

struct PopulateFile {
    const char *key;
    KID kid;

    const char *name;
    int64_t size;

    Span<const char> header;
    Span<const char> nonce;
};

struct MarkData {
    const char *key;
    KID kid;

    int64_t uploaded;
};

static const int64_t StaleDelay = 7 * 86400000ull; // 7 days
static const int64_t CleanupDelay = 1 * 3600000ull; // 1 hour

static const int64_t HeaderLength = 149;
static const int64_t FragmentSize = Mebibytes(4);
static const Size ChunkSize = Kibibytes(64);

static const int CodeNameWords = 2;
static const int CodeNameDigits = 3;

static_assert(FragmentSize % ChunkSize == 0);
static_assert(crypto_secretbox_xsalsa20poly1305_NONCEBYTES == 24);
static_assert(crypto_secretbox_xsalsa20poly1305_MACBYTES == 16);

static s3_Client s3;

static int64_t next_cleanup = 0;
static std::thread cleanup_thread;
static bool cleanup_exit = false;

bool InitDrops()
{
    K_ASSERT(config.s3.Validate());
    return s3.Open(config.s3);
}

void ExitDrops()
{
    if (cleanup_thread.joinable()) {
        cleanup_exit = true;
        cleanup_thread.join();
    }
}

static void CleanupFragments(int64_t now)
{
    BlockAllocator temp_alloc;

    // Clean up old S3 fragments
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT f.kid, f.size, f.split
                           FROM drops d
                           INNER JOIN files f ON (f.parent = d.id)
                           WHERE d.deleted = 1)", &stmt, now - StaleDelay))
            return;

        while (stmt.Step() && !cleanup_exit) {
            K_ASSERT(sqlite3_column_bytes(stmt, 0) == K_SIZE(KID));

            KID kid;
            memcpy(&kid, sqlite3_column_blob(stmt, 0), K_SIZE(KID));

            int64_t size = sqlite3_column_int64(stmt, 1);
            int64_t split = sqlite3_column_int64(stmt, 2);
            int64_t fragments = (size + split - 1) / split;

            bool success = true;

            for (int64_t start = 0; start < fragments; start += 1000) {
                LocalArray<const char *, 1000> keys;

                int64_t end = std::min(start + 1000, fragments);

                for (int64_t i = start; i < end; i++) {
                    const char *key = Fmt(&temp_alloc, "%1/%2", kid, FmtInt(i, 6)).ptr;
                    keys.Append(key);
                }

                success &= s3.DeleteObjects(keys);
            }

            if (success) {
                // Nothing should fail in this function, but if something goes wrong it will be logged
                // and there's nothing else we can do about it, except retry later.
                db.Run("DELETE FROM files WHERE kid = ?1", sq_Binding(kid.raw));
            }
        }
    }

    // Clean empty drops
    db.Run(R"(DELETE FROM drops
              WHERE complete = 1 AND
                    id NOT IN (SELECT parent FROM files))");

    // Recompute per-user usage just in case
    db.Run(R"(UPDATE quotas SET usage = agg.usage
              FROM (SELECT owner AS user, SUM(total) AS usage FROM drops WHERE deleted = 0 GROUP BY owner) AS agg
              WHERE quotas.user = agg.user)");
}

bool PruneDrops()
{
    int64_t now = GetUnixTime();
    int64_t clock = GetMonotonicClock();

    // Deleted expired drops
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(UPDATE drops SET deleted = 1
                           WHERE expire <= ?1 AND deleted = 0
                           RETURNING owner, total)",
                        &stmt, now - StaleDelay))
            return false;

        while (stmt.Step()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            int64_t deleted = sqlite3_column_int64(stmt, 1);

            if (!db.Run("UPDATE quotas SET usage = usage - ?2 WHERE user = ?1", userid, deleted))
                return false;
        }
    }

    if (clock >= next_cleanup) {
        if (cleanup_thread.joinable()) {
            cleanup_exit = true;
            cleanup_thread.join();
        }

        cleanup_exit = false;
        cleanup_thread = std::thread(CleanupFragments, now);

        next_cleanup = clock + CleanupDelay;
    }

    return true;
}

void HandleDropList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT kid_str(d.kid), d.ctime, d.name, d.protect,
                              IFNULL(d.expire, -1) AS expire, d.complete, COUNT(f.id) AS files, d.total,
                              q.usage
                       FROM drops d
                       LEFT JOIN files f ON (f.parent = d.id)
                       LEFT JOIN quotas q ON (q.user = d.owner)
                       WHERE d.owner = ?1 AND
                             d.deleted = 0
                       GROUP BY d.id
                       ORDER BY d.id, f.sequence)",
                    &stmt, session->userid))
        return;
    if (!stmt.Run())
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("quota"); json->Int64(config.quota);

        // User consumption
        {
            int64_t usage = stmt.IsRow() ? sqlite3_column_int64(stmt, 8) : 0;
            json->Key("usage"); json->Int64(usage);
        }

        json->Key("drops"); json->StartArray();
        if (stmt.IsRow()) {
            do {
                const char *kid = (const char *)sqlite3_column_text(stmt, 0);
                int64_t ctime = sqlite3_column_int64(stmt, 1);
                const char *name = (const char *)sqlite3_column_text(stmt, 2);
                bool protect = sqlite3_column_int(stmt, 3);
                int64_t expire = sqlite3_column_int64(stmt, 4);
                bool complete = sqlite3_column_int(stmt, 5);
                int files = sqlite3_column_int(stmt, 6);
                int64_t total = sqlite3_column_int64(stmt, 7);

                if (!files)
                    continue;

                json->StartObject();

                json->Key("kid"); json->String(kid);
                json->Key("ctime"); json->Int64(ctime);
                json->Key("name"); json->String(name);
                json->Key("protect"); json->Bool(protect);
                if (expire >= 0) {
                    json->Key("expire"); json->Int64(expire);
                } else {
                    json->Key("expire"); json->Null();
                }
                json->Key("complete"); json->Bool(complete);
                json->Key("files"); json->Int(files);
                json->Key("total"); json->Int64(total);

                json->EndObject();
            } while (stmt.Step());
            if (!stmt.IsValid())
                return;
        }
        json->EndArray();

        json->EndObject();
    });
}

void HandleDropInfo(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    KID kid;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT kid_str(d.kid) AS kid, d.ctime, d.name, d.protect,
                              IFNULL(d.expire, -1) AS expire, d.complete, d.owner, d.total,
                              kid_str(f.kid), f.name, f.size, f.split, f.header, f.nonce
                       FROM drops d
                       LEFT JOIN files f ON (f.parent = d.id)
                       WHERE d.kid = ?1 AND
                             IIF(d.expire IS NOT NULL, d.expire > ?2, 1) AND
                             d.deleted = 0
                       ORDER BY f.sequence)",
                    &stmt, sq_Binding(kid.raw), now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop KID '%1'", kid);
            io->SendError(404);
        }
        return;
    }

    const char *id = (const char *)sqlite3_column_text(stmt, 0);
    int64_t ctime = sqlite3_column_int64(stmt, 1);
    const char *name = (const char *)sqlite3_column_text(stmt, 2);
    bool protect = sqlite3_column_int(stmt, 3);
    int64_t expire = sqlite3_column_int64(stmt, 4);
    bool complete = sqlite3_column_int(stmt, 5);
    int64_t total = sqlite3_column_int64(stmt, 7);

    if (!complete) {
        RetainPtr<const SessionInfo> session = GetNormalSession(io);
        int64_t owner = sqlite3_column_int64(stmt, 6);

        if (!session || session->userid != owner) {
            LogError("Unknown drop KID '%1'", kid);
            io->SendError(404);
            return;
        }
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("kid"); json->String(id);
        json->Key("ctime"); json->Int64(ctime);
        json->Key("name"); json->String(name);
        if (expire >= 0) {
            json->Key("expire"); json->Int64(expire);
        } else {
            json->Key("expire"); json->Null();
        }
        json->Key("protect"); json->Bool(protect);
        json->Key("complete"); json->Bool(complete);

        json->Key("files"); json->StartArray();
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            do {
                const char *id = (const char *)sqlite3_column_text(stmt, 8);
                const char *name = (const char *)sqlite3_column_text(stmt, 9);
                int64_t size = sqlite3_column_int64(stmt, 10);
                int64_t split = sqlite3_column_int64(stmt, 11);
                const char *header = (const char *)sqlite3_column_text(stmt, 12);
                const char *nonce = (const char *)sqlite3_column_text(stmt, 13);

                json->StartObject();
                json->Key("kid"); json->String(id);
                json->Key("name"); json->String(name);
                json->Key("size"); json->Int64(size);
                json->Key("split"); json->Int64(split);
                json->Key("header"); json->String(header);
                json->Key("nonce"); json->String(nonce);
                json->EndObject();
            } while (stmt.Step());
            if (!stmt.IsValid())
                return;
        }
        json->EndArray();

        json->Key("total"); json->Int64(total);

        json->EndObject();
    });
}

void HandleDropCreate(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    const char *name = nullptr;
    int64_t expiration = -1;
    bool protect = false;
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "name") {
                    json->ParseString(&name);
                } else if (key == "expiration") {
                    json->SkipNull() || json->ParseInt(&expiration);
                } else if (key == "protect") {
                    json->ParseBool(&protect);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!IsStringValid(name)) {
                    LogError("Invalid 'name' parameter");
                    valid = false;
                }
                if (expiration < 0 && !config.allow_infinite) {
                    LogError("You must set an expiration delay");
                    valid = false;
                } else if (!std::find(config.durations.begin(), config.durations.end(), expiration)) {
                    LogError("Unsupported expiration delay");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    int64_t now = GetUnixTime();
    int64_t expire = (expiration >= 0) ? now + expiration : -1;

    KID kid;
    FillKID(KIDType::Drop, &kid);

    if (!db.Run(R"(INSERT INTO drops (kid, owner, ctime, name, expire, protect, total, complete, deleted)
                   VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0, 0, 0))",
                sq_Binding(kid.raw), session->userid, now, name,
                expire >= 0 ? sq_Binding(expire) : sq_Binding(), 0 + protect))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        char str[128];
        Fmt(str, "%1", kid);

        json->Key("kid"); json->String(str);
        if (expire >= 0) {
            json->Key("expire"); json->Int64(expire);
        } else {
            json->Key("expire"); json->Null();
        }

        json->EndObject();
    });
}

void HandleDropPopulate(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;
    HeapArray<PopulateFile> files;
    int64_t total = 0;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    // Process POST data
    {
        bool success = http_ParseJson(io, Mebibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseArray(); json->InArray(); ) {
                PopulateFile file = {};

                for (json->ParseObject(); json->InObject(); ) {
                    Span<const char> key = json->ParseKey();

                    if (key == "kid") {
                        json->ParseString(&file.key);
                    } else if (key == "name") {
                        json->ParseString(&file.name);
                    } else if (key == "size") {
                        json->ParseInt(&file.size);
                    } else if (key == "header") {
                        json->ParseString(&file.header);
                    } else if (key == "nonce") {
                        json->ParseString(&file.nonce);
                    } else {
                        json->UnexpectedKey(key);
                        valid = false;
                    }
                }

                files.Append(file);
                total += file.size;
            }
            valid &= json->IsValid();

            if (valid) {
                for (PopulateFile &file: files) {
                    if (file.key) {
                        valid &= ParseKID(file.key, KIDType::File, &file.kid);
                    } else {
                        FillKID(KIDType::File, &file.kid);
                    }

                    if (!IsFileNameValid(file.name)) {
                        LogError("Invalid file name value");
                        valid = false;
                    }
                    if (file.size < 0) {
                        LogError("Invalid file size (negative)");
                        valid = false;
                    }
                    if (file.header.len != HeaderLength || !IsStringValid(file.header, "\n")) {
                        LogError("Invalid file header value");
                        valid = false;
                    }
                    if (!IsStringValid(file.nonce)) {
                        LogError("Invalid file nonce value");
                        valid = false;
                    }
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    uint8_t changeset[32];
    FillRandomSafe(changeset);

    bool success = db.Transaction([&]() {
        int64_t drop = 0;

        // Find existing drop data
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT id, total
                               FROM drops
                               WHERE kid = ?1 AND
                                     owner = ?2 AND
                                     deleted = 0 AND
                                     complete = 0)",
                            &stmt, sq_Binding(kid.raw), session->userid))
                return false;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Unknown drop KID '%1'", kid);
                    io->SendError(404);
                }
                return false;
            }

            drop = sqlite3_column_int64(stmt, 0);
            total -= sqlite3_column_int64(stmt, 1);
        }

        // Update and check user quota
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(INSERT INTO quotas (user, usage)
                               VALUES (?1, ?2)
                               ON CONFLICT DO UPDATE SET usage = usage + excluded.usage
                               RETURNING usage)",
                            &stmt, session->userid, total))
                return false;

            if (!stmt.Step()) {
                K_ASSERT(!stmt.IsValid());
                return false;
            }

            int64_t usage = sqlite3_column_int64(stmt, 0);
            int64_t remain = config.quota - usage;

            if (remain < 0) {
                LogError("These files would exceed total quota by %1 (max = %2)", FmtDiskSize(-remain), FmtDiskSize(config.quota));
                io->SendError(403);
                return false;
            }
        }

        if (!db.Run("UPDATE drops SET total = ?2 WHERE id = ?1", drop, total))
            return false;

        for (Size i = 0; i < files.len; i++) {
            const PopulateFile &file = files[i];

            if (!db.Run(R"(INSERT INTO files (kid, parent, name, size, split,
                                              header, nonce, sequence, uploaded, changeset)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0, ?9)
                           ON CONFLICT (kid) DO UPDATE SET parent = IIF(parent = excluded.parent, parent, NULL),
                                                           uploaded = IIF(size = excluded.size, uploaded, 0),
                                                           size = excluded.size,
                                                           changeset = excluded.changeset)",
                        sq_Binding(file.kid.raw), drop, file.name, file.size, FragmentSize,
                        file.header, file.nonce, i, sq_Binding(changeset)))
                return false;
        }

        if (!db.Run(R"(DELETE FROM files
                       WHERE parent = ?1 AND changeset IS NOT ?2)",
                    drop, sq_Binding(changeset)))
            return false;

        return true;
    });
    if (!success)
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        for (const PopulateFile &file: files) {
            json->StartObject();

            char str[128];
            Fmt(str, "%1", file.kid);

            json->Key("kid"); json->String(str);
            json->Key("size"); json->Int64(file.size);
            json->Key("split"); json->Int64(FragmentSize);

            json->EndObject();
        }

        json->EndArray();
    });
}

void HandleDropMark(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;
    int64_t uploaded = -1;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::File, &kid)) {
        io->SendError(422);
        return;
    }

    // Process POST data
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "uploaded") {
                    json->ParseInt(&uploaded);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (uploaded < 0) {
                    LogError("Invalid or missing uploaded value");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    bool success = db.Transaction([&]() {
        int64_t file;
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT f.id
                               FROM files f
                               INNER JOIN drops d ON (d.id = f.parent)
                               WHERE f.kid = ?1 AND
                                     d.owner = ?2 AND
                                     d.deleted = 0 AND
                                     d.complete = 0)",
                            &stmt, sq_Binding(kid.raw), session->userid))
                return false;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Unknown drop KID '%1'", kid);
                    io->SendError(404);
                }
                return false;
            }

            file = sqlite3_column_int64(stmt, 0);
        }

        if (!db.Run("UPDATE files SET uploaded = ?2 WHERE id = ?1", file, uploaded))
            return false;

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleDropComplete(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    // Mark drop as complete
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(UPDATE drops SET complete = 1
                           WHERE kid = ?1 AND
                                 owner = ?2 AND
                                 deleted = 0
                           RETURNING id)",
                        &stmt, sq_Binding(kid.raw), session->userid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown drop KID '%1'", kid);
                io->SendError(404);
            }
            return;
        }
    }

    io->SendText(200, "{}", "application/json");
}

void HandleDropDelete(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(UPDATE drops SET deleted = 1
                           WHERE kid = ?1 AND owner = ?2 AND deleted = 0
                           RETURNING total)",
                        &stmt, sq_Binding(kid.raw), session->userid))
            return false;

        if (stmt.Step()) {
            int64_t deleted = sqlite3_column_int64(stmt, 0);

            if (!db.Run("UPDATE quotas SET usage = usage - ?2 WHERE user = ?1", session->userid, deleted))
                return false;
        } else if (!stmt.IsValid()) {
            return false;
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

template <typename T>
static inline T ComputedEncryptedSize(T size)
{
    T extra = (size + ChunkSize - 1) / ChunkSize * 16;
    return size + extra;
}

void HandleFragmentDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    KID kid;
    int64_t fragment;

    // We need to accept drop KIDs for some time, because older drops use them.
    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, &kid)) {
        io->SendError(422);
        return;
    }
    if (kid.type != (int8_t)KIDType::Drop && kid.type != (int8_t)KIDType::File) {
        LogError("Unexpected KID type");
        io->SendError(422);
        return;
    }

    if (const char *str = request.GetQueryValue("fragment"); !ParseInt(str, &fragment)) {
        io->SendError(422);
        return;
    } else if (fragment < 0) {
        LogError("Invalid 'fragment' parameter");
        io->SendError(422);
        return;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT f.size, f.split
                       FROM files f
                       INNER JOIN drops d ON (d.id = f.parent)
                       WHERE f.kid = ?1 AND
                             IIF(d.expire IS NOT NULL, d.expire > ?2, 1) AND
                             d.deleted = 0 AND
                             d.complete = 1)",
                    &stmt, sq_Binding(kid.raw), now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown file KID '%1'", kid);
            io->SendError(404);
        }
        return;
    }

    int64_t size = sqlite3_column_int64(stmt, 0);
    int64_t split = sqlite3_column_int64(stmt, 1);
    int64_t fragments = (size + split - 1) / split;
    int64_t expected = std::min(size - fragment * split, split);

    if (fragment >= fragments) {
        LogError("Excessive fragment index %1", fragment);
        io->SendError(422);
        return;
    }

    StreamWriter writer;
    if (!io->OpenForWrite(200, ComputedEncryptedSize(expected), &writer))
        return;

    Span<const char> key = Fmt(io->Allocator(), "%1/%2", kid, FmtInt(fragment, 6));

    int64_t downloaded = s3.GetObject(key, [&](int64_t offset, Span<const uint8_t> buf) {
        if (offset != writer.GetRawWritten()) {
            LogError("Transient S3 download error, please retry");
            return false;
        }

        return writer.Write(buf);
    });
    if (downloaded < 0)
        return;

    writer.Close();
}

void HandleFragmentUpload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;
    int64_t fragment;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::File, &kid)) {
        io->SendError(422);
        return;
    }
    if (const char *str = request.GetQueryValue("fragment"); !ParseInt(str, &fragment)) {
        io->SendError(422);
        return;
    } else if (fragment < 0) {
        LogError("Invalid 'fragment' parameter");
        io->SendError(422);
        return;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT f.size, f.split
                       FROM files f
                       INNER JOIN drops d ON (d.id = f.parent)
                       WHERE f.kid = ?1 AND
                             d.owner = ?2 AND
                             IIF(d.expire IS NOT NULL, d.expire > ?3, 1) AND
                             d.deleted = 0 AND
                             d.complete = 0)",
                    &stmt, sq_Binding(kid.raw), session->userid, now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown file KID '%1'", kid);
            io->SendError(404);
        }
        return;
    }

    int64_t size = sqlite3_column_int64(stmt, 0);
    int64_t split = sqlite3_column_int64(stmt, 1);
    int64_t fragments = (size + split - 1) / split;
    int64_t expected = std::min(size - fragment * split, split);

    if (fragment >= fragments) {
        LogError("Excessive fragment index %1", fragment);
        io->SendError(422);
        return;
    }
    if (request.GetBodyLength() != ComputedEncryptedSize(expected)) {
        LogError("Unexpected fragment size, expected %1", FmtMemSize(ComputedEncryptedSize(expected)));
        io->SendError(422);
        return;
    }

    StreamReader reader;
    if (!io->OpenForRead(-1, &reader))
        return;

    Span<const char> key = Fmt(io->Allocator(), "%1/%2", kid, FmtInt(fragment, 6));

    s3_PutResult ret = s3.PutObject(key, ComputedEncryptedSize(expected), [&](int64_t offset, Span<uint8_t> buf) {
        if (offset != reader.GetRawRead()) {
            LogError("Transient S3 upload error, please retry");
            return (Size)-1;
        }

        return reader.Read(buf);
    });
    if (ret == s3_PutResult::OtherError) {
        io->SendError(503);
        return;
    }

    io->SendText(200, "{}", "application/json");
}

void HandleFileDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    KID kid;
    Size idx = 0;
    {
        K_ASSERT(StartsWith(request.path, "/download/"));

        Span<const char> remain = request.path + 10;
        Span<const char> first = SplitStr(remain, '/', &remain);

        if (!ParseKID(first, &kid)) {
            io->SendError(422);
            return;
        }

        if (remain.len && !ParseInt(remain, &idx)) {
            io->SendError(422);
            return;
        }
    }

    switch ((KIDType)kid.type) {
        case KIDType::Drop: {
            if (idx < 0) {
                LogError("Cannot use negative file index");
                io->SendError(422);
                return;
            }

            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT f.kid
                               FROM drops d
                               INNER JOIN files f ON (f.parent = d.id)
                               WHERE d.kid = ?1 AND f.sequence = ?2)",
                            &stmt, sq_Binding(kid.raw), idx))
                return;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Unknown drop or file KID '%1'", kid);
                    io->SendError(404);
                }
                return;
            }

            // Should always be K_SIZE(kid) but let's be cautious
            Size copy_len = std::min((Size)sqlite3_column_bytes(stmt, 0), K_SIZE(kid));

            MemCpy(kid.raw, sqlite3_column_blob(stmt, 0), copy_len);
        } break;

        case KIDType::File: {
            if (idx) {
                LogError("Unexpected file index with file KID");
                io->SendError(422);
                return;
            }
        } break;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT f.name, f.size, f.header, base64(f.nonce), f.split
                       FROM files f
                       INNER JOIN drops d ON (d.id = f.parent)
                       WHERE f.kid = ?1 AND
                             IIF(d.expire IS NOT NULL, d.expire > ?2, 1) AND
                             d.deleted = 0 AND
                             d.complete = 1)",
                    &stmt, sq_Binding(kid.raw), now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop or file KID '%1'", kid);
            io->SendError(404);
        }
        return;
    }

    const char *name = (const char *)sqlite3_column_text(stmt, 0);
    int64_t size = sqlite3_column_int64(stmt, 1);
    Span<const char> header = MakeSpan((const char *)sqlite3_column_text(stmt, 2), sqlite3_column_bytes(stmt, 2));
    Span<const uint8_t> nonce = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 3), sqlite3_column_bytes(stmt, 3));
    int64_t split = sqlite3_column_int64(stmt, 4);

    int64_t fragments = (size + split - 1) / split;
    int64_t total = header.len + 1 + nonce.len + ComputedEncryptedSize(size);

    const char *disposition = Fmt(io->Allocator(), "attachment; filename=\"%1.age\"; filename*=UTF-8''%2.age", FmtEscape(name, '"'), FmtUrlSafe(name, "-._~@")).ptr;
    io->AddHeader("Content-Disposition", disposition);

    StreamWriter writer;
    if (!io->OpenForWrite(200, total, &writer))
        return;

    writer.Write(header);
    writer.Write('\n');
    writer.Write(nonce);

    // Send encoded fragments
    {
        Span<uint8_t> buf = AllocateSpan<uint8_t>(io->Allocator(), ComputedEncryptedSize(size));

        for (int64_t i = 0; i < fragments; i++) {
            Span<const char> key = Fmt(io->Allocator(), "%1/%2", kid, FmtInt(i, 6));
            Size downloaded = s3.GetObject(key, buf);

            if (downloaded < 0)
                return;

            writer.Write(buf.ptr, downloaded);
        }
    }

    writer.Close();
}

void HandleDropCodeName(http_IO *io)
{
    const char *language = GetThreadLocale();
    Span<const char *const> words = CodeWords.FindValue(language, Words0);

    LocalArray<char, 512> codename;

    FastRandom rng;

    int used[16] = {};
    static_assert(CodeNameWords > 0);
    static_assert(CodeNameWords <= K_LEN(used));

    for (int i = 0; i < CodeNameWords; i++) {
        int rnd = 0;
        do {
            rnd = rng.GetInt(0, words.len);
        } while (std::any_of(used, used + i, [&](int prev) { return prev == rnd; }));
        used[i] = rnd;

        codename.Append(words[rnd]);
        codename.Append('_');
    }

    if (CodeNameDigits) {
        codename.len += Fmt(codename.TakeAvailable(), "%1", FmtRandom(CodeNameDigits, "0123456789")).len;
    } else {
        codename.len--;
    }

    const char *json = Fmt(io->Allocator(), "{\"codename\": \"%1\"}", codename).ptr;
    io->SendText(200, json, "application/json");
}

}
