// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(WEB_DROP)

#include "lib/native/base/base.hh"
#include "lib/native/sqlite/sqlite.hh"
#include "web.hh"
#include "user.hh"
#include "utility.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static const int64_t MaxExpiration = 90 * 86400000ull; // 90 days
static bool AllowNoExpiration = true;
static const int64_t StaleDelay = 7 * 86400000ull; // 7 days
static const int64_t CleanupDelay = 6 * 3600000ull; // 6 hours

static const int64_t HeaderLength = 149;
static const int64_t FragmentSize = Mebibytes(2);
static const Size ChunkSize = Kibibytes(64);

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
        if (!db.Prepare("SELECT kid, size, split FROM drops WHERE deleted = 1", &stmt, now - StaleDelay))
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
                    const char *key = Fmt(&temp_alloc, "%1%2/%3", config.drop_prefix.value, kid, FmtInt(i, 6)).ptr;
                    keys.Append(key);
                }

                success &= s3.DeleteObjects(keys);
            }

            if (success) {
                // Nothing should fail in this function, but if something goes wrong it will be logged
                // and there's nothing else we can do about it, except retry later.
                db.Run("DELETE FROM drops WHERE kid = ?1", sq_Binding(kid.raw));
            }
        }
    }

    // Recompute per-user usage just in case
    db.Run(R"(UPDATE quotas SET total = agg.total
              FROM (SELECT owner AS user, SUM(size) AS total FROM drops WHERE deleted = 0 GROUP BY owner) AS agg
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
                           RETURNING owner, size)",
                        &stmt, now - StaleDelay))
            return false;

        while (stmt.Step()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            int64_t size = sqlite3_column_int64(stmt, 1);

            if (!db.Run("UPDATE quotas SET total = total - ?2 WHERE user = ?1", userid, size))
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
    if (!db.Prepare(R"(SELECT kid_str(d.kid), d.name, d.size, d.protect, IFNULL(d.expire, -1),
                              IIF(d.uploaded = d.size, 1, 0) AS complete,
                              q.total
                       FROM drops d
                       LEFT JOIN quotas q ON (user = 1)
                       WHERE owner = ?1 AND deleted = 0)", &stmt, session->userid))
        return;
    if (!stmt.Run())
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("quota"); json->Int64(config.drop_quota.value);

        // User consumption
        {
            int64_t total = stmt.IsRow() ? sqlite3_column_int64(stmt, 6) : 0;
            json->Key("total"); json->Int64(total);
        }

        json->Key("drops"); json->StartArray();
        if (stmt.IsRow()) {
            do {
                const char *kid = (const char *)sqlite3_column_text(stmt, 0);
                const char *name = (const char *)sqlite3_column_text(stmt, 1);
                int64_t size = sqlite3_column_int64(stmt, 2);
                bool protect = sqlite3_column_int(stmt, 3);
                int64_t expire = sqlite3_column_int64(stmt, 4);
                bool complete = sqlite3_column_int(stmt, 5);

                json->StartObject();

                json->Key("kid"); json->String(kid);
                json->Key("name"); json->String(name);
                json->Key("size"); json->Int64(size);
                json->Key("protect"); json->Bool(protect);
                if (expire >= 0) {
                    json->Key("expire"); json->Int64(expire);
                } else {
                    json->Key("expire"); json->Null();
                }
                json->Key("complete"); json->Bool(complete);

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
    if (!db.Prepare(R"(SELECT kid_str(kid), name, size, protect, header, nonce, split
                       FROM drops WHERE kid = ?1 AND
                                        IIF(expire IS NOT NULL, expire > ?2, 1) AND
                                        deleted = 0 AND
                                        uploaded = size)",
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
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    int64_t size = sqlite3_column_int64(stmt, 2);
    bool protect = sqlite3_column_int(stmt, 3);
    const char *header = (const char *)sqlite3_column_text(stmt, 4);
    const char *nonce = (const char *)sqlite3_column_text(stmt, 5);
    int64_t split = sqlite3_column_int64(stmt, 6);

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("kid"); json->String(id);
        json->Key("name"); json->String(name);
        json->Key("size"); json->Int64(size);
        json->Key("protect"); json->Bool(protect);
        json->Key("header"); json->String(header);
        json->Key("nonce"); json->String(nonce);
        json->Key("split"); json->Int64(split);

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
    int64_t size = -1;
    int64_t expiration = -1;
    bool protect = false;
    Span<const char> header = {};
    Span<const char> nonce = {};
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "name") {
                    json->ParseString(&name);
                } else if (key == "size") {
                    json->ParseInt(&size);
                } else if (key == "expiration") {
                    json->SkipNull() || json->ParseInt(&expiration);
                } else if (key == "protect") {
                    json->ParseBool(&protect);
                } else if (key == "header") {
                    json->ParseString(&header);
                } else if (key == "nonce") {
                    json->ParseString(&nonce);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!name || !IsStringValid(name)) {
                    LogError("Invalid 'name' parameter");
                    valid = false;
                }
                if (size <= 0) {
                    LogError("Invalid or excessive 'size' parameter");
                    valid = false;
                }
                if (expiration < 0 && !AllowNoExpiration) {
                    LogError("You must set an expiration time");
                    valid = false;
                } else if (expiration > MaxExpiration) {
                    LogError("Excessive expiration time, max is approximately %1 days", MaxExpiration / 86400000);
                    valid = false;
                }
                if (header.len != HeaderLength || !IsStringValid(header, "\n")) {
                    LogError("Invalid 'header' parameter");
                    valid = false;
                }
                if (!IsStringValid(nonce)) {
                    LogError("Invalid 'nonce' parameter");
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
    int64_t expire = now + expiration;

    KID kid;
    FillKID(KIDType::Drop, &kid);

    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO quotas (user, total) 
                           VALUES (?1, ?2)
                           ON CONFLICT DO UPDATE SET total = total + excluded.total
                           RETURNING total)",
                        &stmt, session->userid, size))
            return false;

        if (!stmt.Step()) {
            K_ASSERT(!stmt.IsValid());
            return false;
        }

        // Make sure we're not over quota
        {
            int64_t total = sqlite3_column_int64(stmt, 0);
            int64_t remain = config.drop_quota.value - total;

            if (remain < 0) {
                LogError("These files would exceed total quota by %1 (max = %2)", FmtDiskSize(-remain), FmtDiskSize(config.drop_quota.value));
                io->SendError(403);
                return false;
            }
        }

        if (!db.Run(R"(INSERT INTO drops (kid, owner, name, size, expire, protect,
                                          header, nonce, split, uploaded, deleted)
                       VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 0, 0))",
                    sq_Binding(kid.raw), session->userid, name, size, expire,
                    0 + protect, header, nonce, FragmentSize))
            return false;

        return true;
    });
    if (!success)
        return;

    Span<const char> json = Fmt(io->Allocator(), "{\"kid\": \"%1\", \"split\": %2}", kid, FragmentSize);
    io->SendText(200, json, "application/json");
}

void HandleDropDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    KID kid;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            const char *str = nullptr;
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "kid") {
                    json->ParseString(&str);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                valid &= ParseKID(str, KIDType::Drop, &kid);
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(UPDATE drops SET deleted = 1
                           WHERE kid = ?1 AND owner = ?2 AND deleted = 0
                           RETURNING size)",
                        &stmt, sq_Binding(kid.raw), session->userid))
            return false;

        if (stmt.Step()) {
            int64_t size = sqlite3_column_int64(stmt, 0);

            if (!db.Run("UPDATE quotas SET total = total - ?2 WHERE user = ?1", session->userid, size))
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

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
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
    if (!db.Prepare(R"(SELECT size, split
                       FROM drops WHERE kid = ?1 AND
                                        owner = ?2 AND
                                        IIF(expire IS NOT NULL, expire > ?3, 1) AND
                                        deleted = 0 AND
                                        uploaded < size)",
                    &stmt, sq_Binding(kid.raw), session->userid, now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop KID '%1'", kid);
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

    Span<const char> key = Fmt(io->Allocator(), "%1%2/%3", config.drop_prefix.value, kid, FmtInt(fragment, 6));

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

void HandleFragmentDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    KID kid;
    int64_t fragment;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
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
    if (!db.Prepare(R"(SELECT size, split
                       FROM drops WHERE kid = ?1 AND
                                        IIF(expire IS NOT NULL, expire > ?2, 1) AND
                                        deleted = 0 AND
                                        uploaded = size)",
                    &stmt, sq_Binding(kid.raw), now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop KID '%1'", kid);
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

    Span<const char> key = Fmt(io->Allocator(), "%1%2/%3", config.drop_prefix.value, kid, FmtInt(fragment, 6));

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

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    // POST data
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
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
                    LogError("Invalid or missing 'uploaded' parameter");
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

    sq_Statement stmt;
    if (!db.Prepare(R"(UPDATE drops SET uploaded = IIF(?4 <= size, ?4, uploaded)
                       WHERE kid = ?1 AND
                             owner = ?2 AND
                             IIF(expire IS NOT NULL, expire > ?3, 1) AND
                             uploaded < size
                       RETURNING size)",
                    &stmt, sq_Binding(kid.raw), session->userid, now, uploaded))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop KID '%1'", kid);
            io->SendError(404);
        }
        return;
    }

    int64_t size = sqlite3_column_int64(stmt, 0);

    if (uploaded > size) {
        LogError("Excessive uploaded value");
        io->SendError(422);
        return;
    }

    io->SendText(200, "{}", "application/json");
}

void HandleDropDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    KID kid;

    if (Span<const char> str = SplitStrReverse(request.path, '/'); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT name, size, header, base64(nonce), split
                       FROM drops WHERE kid = ?1 AND
                                        IIF(expire IS NOT NULL, expire > ?2, 1) AND
                                        deleted = 0 AND
                                        uploaded = size)",
                    &stmt, sq_Binding(kid.raw), now))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown drop KID '%1'", kid);
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

    const char *disposition = Fmt(io->Allocator(), "attachment; filename=\"%1.age\"", FmtEscape(name, '"')).ptr;
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
            Span<const char> key = Fmt(io->Allocator(), "%1%2/%3", config.drop_prefix.value, kid, FmtInt(i, 6));
            Size downloaded = s3.GetObject(key, buf);

            if (downloaded < 0)
                return;

            writer.Write(buf.ptr, downloaded);
        }
    }

    writer.Close();
}

}

#endif
