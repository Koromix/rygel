// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/sqlite/sqlite.hh"
#include "rokkerd.hh"
#include "kid.hh"
#include "user.hh"
#include "utility.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static const int64_t MaxSize = Megabytes(4000);
static const int64_t MaxExpiration = 90 * 86400000ull;
static bool AllowNoExpiration = true;

static const int64_t ChunkSize = Mebibytes(2);
static const Size ChunkSplit = Kibibytes(64);
static const int64_t ChunkExtra = 16;

static_assert(ChunkSize % ChunkSplit == 0);
static_assert(crypto_secretbox_xsalsa20poly1305_NONCEBYTES == 24);
static_assert(crypto_secretbox_xsalsa20poly1305_MACBYTES == 16);

static s3_Client s3;

bool InitDrop()
{
    K_ASSERT(config.s3.remote.Validate());
    return s3.Open(config.s3.remote);
}

bool PruneDrops()
{
    int64_t now = GetUnixTime();

    if (!db.Run("DELETE FROM drops WHERE expire <= ?1", now))
        return false;

    return true;
}

void HandleDropCreate(http_IO *io)
{
    K_ASSERT(config.drop);

    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    const char *name = nullptr;
    int64_t size = -1;
    int64_t expiration = -1;
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
                if (size <= 0 || size > MaxSize) {
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

    if (!db.Run(R"(INSERT INTO drops (kid, owner, name, size, expire, chunk, uploaded)
                   VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0))",
                sq_Binding(kid.raw), session->userid, name, size, expire, ChunkSize))
        return;

    Span<const char> json = Fmt(io->Allocator(), "{\"kid\": \"%1\", \"chunk\": %2}", kid, ChunkSize);
    io->SendText(200, json, "application/json");
}

void HandleDropUpload(http_IO *io)
{
    K_ASSERT(config.drop);

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
    if (!db.Prepare(R"(SELECT size, chunk
                       FROM drops WHERE kid = ?1 AND
                                        owner = ?2 AND
                                        IIF(expire IS NOT NULL, expire > ?3, 1) AND
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
    int64_t chunk = sqlite3_column_int64(stmt, 1);

    int64_t fragments = (size + chunk - 1) / chunk;
    int64_t expected = std::min(size - fragment * chunk, chunk) + ChunkExtra;

    if (fragment >= fragments) {
        LogError("Excessive fragment index %1", fragment);
        io->SendError(422);
        return;
    }
    if (expected != request.GetBodyLength()) {
        LogError("Unexpected fragment size, expected %1", FmtMemSize(expected));
        io->SendError(422);
        return;
    }

    io->ExtendTimeout(30000);

    StreamReader reader;
    if (!io->OpenForRead(-1, &reader))
        return;

    Span<const char> key = Fmt(io->Allocator(), "%1/%2/%3", config.s3.drop_path, kid, fragment);

    s3_PutResult ret = s3.PutObject(key, expected, [&](int64_t offset, Span<uint8_t> buf) {
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

void HandleDropMark(http_IO *io)
{
    K_ASSERT(config.drop);

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

void HandleDropInfo(http_IO *io)
{
    K_ASSERT(config.drop);

    const http_RequestInfo &request = io->Request();

    KID kid;

    if (const char *str = request.GetQueryValue("kid"); !ParseKID(str, KIDType::Drop, &kid)) {
        io->SendError(422);
        return;
    }

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT kid_str(kid), name, size, chunk
                       FROM drops WHERE kid = ?1 AND
                                        IIF(expire IS NOT NULL, expire > ?2, 1) AND
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
    int64_t chunk = sqlite3_column_int64(stmt, 3);

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("kid"); json->String(id);
        json->Key("name"); json->String(name);
        json->Key("size"); json->Int64(size);
        json->Key("chunk"); json->Int64(chunk);

        json->EndObject();
    });
}

void HandleDropFragment(http_IO *io)
{
    K_ASSERT(config.drop);

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
    if (!db.Prepare(R"(SELECT size, chunk
                       FROM drops WHERE kid = ?1 AND
                                        IIF(expire IS NOT NULL, expire > ?2, 1) AND
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
    int64_t chunk = sqlite3_column_int64(stmt, 1);

    int64_t fragments = (size + chunk - 1) / chunk;
    int64_t expected = std::min(size - fragment * chunk, chunk) + ChunkExtra;

    if (fragment >= fragments) {
        LogError("Excessive fragment index %1", fragment);
        io->SendError(422);
        return;
    }

    io->ExtendTimeout(30000);

    StreamWriter writer;
    if (!io->OpenForWrite(200, expected, &writer))
        return;

    Span<const char> key = Fmt(io->Allocator(), "%1/%2/%3", config.s3.drop_path, kid, fragment);

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

}
