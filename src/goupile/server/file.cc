// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "config.hh"
#include "domain.hh"
#include "file.hh"
#include "instance.hh"
#include "goupile.hh"
#include "user.hh"
#include "vendor/libsodium/src/libsodium/include/sodium/crypto_hash_sha256.h"

#if defined(_WIN32)
    #include <io.h>
#endif

namespace K {

struct PublishFile {
    const char *filename;
    const char *sha256;
    const char *bundle;

    K_HASHTABLE_HANDLER(PublishFile, filename);
};

static bool CheckSha256(Span<const char> sha256)
{
    const auto test_char = [](char c) { return (c >= 'A' && c <= 'Z') || IsAsciiDigit(c); };

    if (sha256.len != 64) {
        LogError("Malformed SHA256 (incorrect length)");
        return false;
    }
    if (!std::all_of(sha256.begin(), sha256.end(), test_char)) {
        LogError("Malformed SHA256 (unexpected character)");
        return false;
    }

    return true;
}

static void AddMimeTypeHeader(http_IO *io, const char *filename)
{
   const char *mimetype = GetMimeType(GetPathExtension(filename), nullptr);

    if (mimetype) {
        io->AddHeader("Content-Type", mimetype);
    }
}

bool ServeFile(http_IO *io, InstanceHolder *instance, const char *sha256, const char *filename, bool download, int64_t max_age)
{
    const http_RequestInfo &request = io->Request();

    // Lookup file in database
    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT rowid, compression
                                  FROM fs_objects
                                  WHERE sha256 = ?1)", &stmt, sha256))
        return true;
    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Missing file object");
        }
        return false;
    }

    int64_t rowid = sqlite3_column_int64(stmt, 0);
    const char *compression = (const char *)sqlite3_column_text(stmt, 1);

    // Handle caching
    {
        const char *etag = request.GetHeaderValue("If-None-Match");

        if (etag && TestStr(etag, sha256)) {
            io->SendEmpty(304);
            return false;
        }

        io->AddCachingHeaders(max_age, sha256);
    }

    // Negociate content encoding
    CompressionType src_encoding;
    CompressionType dest_encoding;
    {
        if (!compression || !OptionToEnumI(CompressionTypeNames, compression, &src_encoding)) {
            LogError("Unknown compression type '%1'", compression);
            return false;
        }

        if (!io->NegociateEncoding(src_encoding, &dest_encoding))
            return false;
    }

    // Open file blob
    sqlite3_blob *src_blob;
    Size src_len;
    if (sqlite3_blob_open(*instance->db, "main", "fs_objects", "blob", rowid, 0, &src_blob) != SQLITE_OK) {
        LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
        return false;
    }
    src_len = sqlite3_blob_bytes(src_blob);
    K_DEFER { sqlite3_blob_close(src_blob); };

    if (download) {
        const char *disposition = Fmt(io->Allocator(), "attachment; filename=\"%1\"", FmtUrlSafe(filename, "")).ptr;
        io->AddHeader("Content-Disposition", disposition);
    }

    // Fast path (small objects)
    if (dest_encoding == src_encoding && src_len <= 65536) {

        uint8_t *ptr = (uint8_t *)AllocateRaw(io->Allocator(), src_len);

        if (sqlite3_blob_read(src_blob, ptr, (int)src_len, 0) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
            return false;
        }

        io->AddEncodingHeader(dest_encoding);
        AddMimeTypeHeader(io, filename);

        io->SendBinary(200, MakeSpan(ptr, src_len));

        return false;
    }

    // Handle range requests
    if (src_encoding == CompressionType::None && dest_encoding == src_encoding) {
        LocalArray<http_ByteRange, 16> ranges;
        {
            const char *str = request.GetHeaderValue("Range");

            if (str && !http_ParseRange(str, src_len, &ranges)) {
                io->SendError(416);
                return false;
            }
        }

        if (ranges.len >= 2) {
            char boundary[17];
            {
                uint64_t buf;
                FillRandomSafe(&buf, K_SIZE(buf));
                Fmt(boundary, "%1", FmtHex(buf, 16));
            }

            // Boundary strings
            LocalArray<Span<const char>, K_LEN(ranges.data) * 2> boundaries;
            int64_t total_len = 0;
            {
                const char *mimetype = GetMimeType(GetPathExtension(filename), nullptr);

                for (Size i = 0; i < ranges.len; i++) {
                    const http_ByteRange &range = ranges[i];

                    Span<const char> before;
                    if (mimetype) {
                        before = Fmt(io->Allocator(), "Content-Type: %1\r\n"
                                                      "Content-Range: bytes %2-%3/%4\r\n\r\n",
                                     mimetype, range.start, range.end - 1, src_len);
                    } else {
                        before = Fmt(io->Allocator(), "Content-Range: bytes %1-%2/%3\r\n\r\n",
                                     range.start, range.end - 1, src_len);
                    }

                    Span<const char> after;
                    if (i < ranges.len - 1) {
                        after = Fmt(io->Allocator(), "\r\n--%1\r\n", boundary);
                    } else {
                        after = Fmt(io->Allocator(), "\r\n--%1--\r\n", boundary);
                    }

                    boundaries.Append(before);
                    boundaries.Append(after);

                    total_len += before.len;
                    total_len += range.end - range.start;
                    total_len += after.len;
                }
            }

            // Add headers
            {
                char buf[512];
                Fmt(buf, "multipart/byteranges; boundary=%1", boundary);

                io->AddEncodingHeader(dest_encoding);
                io->AddHeader("Content-Type", buf);
            }

            StreamWriter writer;
            if (!io->OpenForWrite(206, dest_encoding, total_len, &writer))
                return false;

            for (Size i = 0; i < ranges.len; i++) {
                const http_ByteRange &range = ranges[i];
                Size range_len = range.end - range.start;

                writer.Write(boundaries[i * 2]);

                Size offset = 0;
                while (offset < range_len) {
                    uint8_t buf[16384];
                    Size copy_len = std::min(range_len - offset, K_SIZE(buf));

                    if (sqlite3_blob_read(src_blob, buf, (int)copy_len, (int)(range.start + offset)) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                        return false;
                    }

                    writer.Write(buf, copy_len);
                    offset += copy_len;
                }

                writer.Write(boundaries[i * 2 + 1]);
            }
            writer.Close();

            return false;
        } else if (ranges.len == 1) {
            const http_ByteRange &range = ranges[0];
            Size range_len = range.end - range.start;

            // Add headers
            {
                char buf[512];
                Fmt(buf, "bytes %1-%2/%3", range.start, range.end - 1, src_len);

                io->AddHeader("Content-Range", buf);
                io->AddEncodingHeader(dest_encoding);
                AddMimeTypeHeader(io, filename);
            }

            StreamWriter writer;
            if (!io->OpenForWrite(206, dest_encoding, range_len, &writer))
                return  false;

            Size offset = 0;
            while (offset < range_len) {
                uint8_t buf[16384];
                Size copy_len = std::min(range_len - offset, K_SIZE(buf));

                if (sqlite3_blob_read(src_blob, buf, (int)copy_len, (int)(range.start + offset)) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                    return false;
                }

                writer.Write(buf, copy_len);
                offset += copy_len;
            }
            writer.Close();

            return false;
        } else {
            io->AddHeader("Accept-Ranges", "bytes");

            // Go on with default code path
        }
    }

    // Default path, for big files and/or transcoding (Gzip to None, etc.)
    {
        io->AddEncodingHeader(dest_encoding);
        AddMimeTypeHeader(io, filename);

        StreamWriter writer;
        if (src_encoding == dest_encoding) {
            src_encoding = CompressionType::None;

            if (!io->OpenForWrite(200, src_len, &writer))
                return false;
        } else {
            if (!io->OpenForWrite(200, dest_encoding, -1, &writer))
                return false;
        }

        Size offset = 0;
        StreamReader reader([&](Span<uint8_t> buf) {
            Size copy_len = std::min(src_len - offset, buf.len);

            if (sqlite3_blob_read(src_blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                return (Size)-1;
            }

            offset += copy_len;
            return copy_len;
        }, filename, src_encoding);

        // Not much we can do at this stage in case of error. Client will get truncated data.
        SpliceStream(&reader, -1, &writer);
        writer.Close();
    }

    return true;
}

static bool BlobExists(InstanceHolder *instance, const char *sha256)
{
    sq_Statement stmt;
    if (!instance->db->Prepare("SELECT rowid FROM fs_objects WHERE sha256 = ?1", &stmt))
        return false;
    sqlite3_bind_text(stmt, 1, sha256, -1, SQLITE_STATIC);

    return stmt.Step();
}

bool PutFile(http_IO *io, InstanceHolder *instance, CompressionType compression_type,
             const char *expect, const char **out_sha256)
{
    if (expect && BlobExists(instance, expect)) {
        *out_sha256 = DuplicateString(expect, io->Allocator()).ptr;
        return true;
    }

    // Create temporary file
    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(gp_config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
    if (!tmp_filename)
        return false;
    K_DEFER {
        CloseDescriptor(fd);
        UnlinkFile(tmp_filename);
    };

    // Read and compress request body
    int64_t total_len = 0;
    char sha256[65];
    {
        StreamWriter writer(fd, "<temp>", 0, compression_type);
        StreamReader reader;
        if (!io->OpenForRead(instance->settings.max_file_size, &reader))
            return false;

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);
            if (buf.len < 0)
                return false;
            total_len += buf.len;

            if (!writer.Write(buf))
                return false;

            crypto_hash_sha256_update(&state, buf.data, buf.len);
        } while (!reader.IsEOF());
        if (!writer.Close())
            return false;

        uint8_t hash[crypto_hash_sha256_BYTES];
        crypto_hash_sha256_final(&state, hash);
        FormatSha256(hash, sha256);
    }

    // Check checksum
    if (expect) {
        if (!TestStr(sha256, expect)) {
            LogError("Upload refused because of sha256 mismatch");
            io->SendError(422);
            return false;
        }
    } else {
        if (BlobExists(instance, sha256)) {
            *out_sha256 = DuplicateString(sha256, io->Allocator()).ptr;
            return true;
        }
    }

    // Copy to database blob
    {
#if defined(_WIN32)
        int64_t file_len = _lseeki64(fd, 0, SEEK_CUR);
#else
        int64_t file_len = lseek(fd, 0, SEEK_CUR);
#endif

        if (lseek(fd, 0, SEEK_SET) < 0) {
            LogError("lseek('<temp>') failed: %1", strerror(errno));
            return false;
        }

        int64_t mtime = GetUnixTime();

        int64_t rowid;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(INSERT INTO fs_objects (sha256, mtime, compression, size, blob)
                                          VALUES (?1, ?2, ?3, ?4, ?5)
                                          RETURNING rowid)",
                                       &stmt, sha256, mtime, CompressionTypeNames[(int)compression_type],
                                       total_len, sq_Binding::Zeroblob(file_len)))
                return false;

            if (stmt.Step()) {
                rowid = sqlite3_column_int64(stmt, 0);
            } else {
                if (stmt.IsValid()) {
                    LogError("Duplicate file blob '%1'", sha256);
                    io->SendError(409);
                }
                return false;
            }
        }

        sqlite3_blob *blob;
        if (sqlite3_blob_open(*instance->db, "main", "fs_objects", "blob", rowid, 1, &blob) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
            return false;
        }
        K_DEFER { sqlite3_blob_close(blob); };

        StreamReader reader(fd, "<temp>");
        int64_t read_len = 0;

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);
            if (buf.len < 0)
                return false;

            if (buf.len + read_len > file_len) {
                LogError("Temporary file size has changed (bigger)");
                return false;
            }
            if (sqlite3_blob_write(blob, buf.data, (int)buf.len, (int)read_len) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                return false;
            }

            read_len += buf.len;
        } while (!reader.IsEOF());
        if (read_len < file_len) {
            LogError("Temporary file size has changed (truncated)");
            return false;
        }
    }

    *out_sha256 = DuplicateString(sha256, io->Allocator()).ptr;
    return true;
}

void HandleFileList(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.allow_guests && !instance->settings.use_offline) {
        RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
        const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

        if (!session) {
            LogError("User is not logged in");
            io->SendError(401);
            return;
        }
        if (!stamp) {
            LogError("User is not allowed to list files");
            io->SendError(403);
            return;
        }
    }

    if (instance->master != instance) {
        LogError("Cannot list files through slave instance");
        io->SendError(403);
        return;
    }

    int64_t fs_version;
    if (const char *str = request.GetQueryValue("version"); str) {
        if (!ParseInt(str, &fs_version)) {
            io->SendError(422);
            return;
        }

        if (fs_version == 0) {
            RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

            if (!session || !session->HasPermission(instance, UserPermission::BuildCode)) {
                LogError("You cannot access pages in development");
                io->SendError(403);
                return;
            }
        }
    } else {
        fs_version = instance->fs_version.load(std::memory_order_relaxed);
    }

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256, i.bundle
                                  FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1
                                  ORDER BY i.filename)", &stmt))
        return;
    sqlite3_bind_int64(stmt, 1, fs_version);

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("version"); json->Int64(fs_version);
        json->Key("files"); json->StartArray();
        while (stmt.Step()) {
            const char *filename = (const char *)sqlite3_column_text(stmt, 0);
            int64_t size = sqlite3_column_int64(stmt, 1);
            const char *sha256 = (const char *)sqlite3_column_text(stmt, 2);
            const char *bundle = (const char *)sqlite3_column_text(stmt, 3);

            json->StartObject();
            json->Key("filename"); json->String(filename);
            json->Key("size"); json->Int64(size);
            json->Key("sha256"); json->String(sha256);
            if (bundle) {
                json->Key("bundle"); json->String(bundle);
            } else {
                json->Key("bundle"); json->Null();
            }
            json->EndObject();
        }
        if (!stmt.IsValid())
            return;
        json->EndArray();

        json->EndObject();
    });
}

// Returns true when request has been handled (file exists or an error has occured)
bool HandleFileGet(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    const char *url = request.path + 1 + instance->key.len;

    K_ASSERT(url <= request.path + strlen(request.path));
    K_ASSERT(url[0] == '/');

    const char *client_sha256 = request.GetQueryValue("sha256");

    // Handle various paths
    if (TestStr(url, "/favicon.png")) {
        url ="/files/favicon.png";
    } else if (TestStr(url, "/manifest.json")) {
        url = "/files/manifest.json";
    } else if (!instance->settings.allow_guests && !instance->settings.use_offline) {
        RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
        const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

        if (!stamp)
            return false;
    }
    if (!StartsWith(url, "/files/"))
        return false;

    if (instance->master != instance) {
        LogError("Cannot get files through slave instance");
        io->SendError(403);
        return true;
    }

    Span<const char> filename = url + 7;

    int64_t fs_version;
    bool explicit_version;
    {
        Span<const char> remain;

        if (ParseInt(filename, &fs_version, 0, &remain) && remain.ptr[0] == '/') {
            if (fs_version == 0) {
                RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

                if (!session || !session->HasPermission(instance, UserPermission::BuildCode)) {
                    LogError("You cannot access pages in development");
                    io->SendError(403);
                    return true;
                }
            }

            filename = remain.Take(1, remain.len - 1);
            explicit_version = true;
        } else {
            fs_version = instance->fs_version.load(std::memory_order_relaxed);
            explicit_version = false;
        }
    }

    bool bundle = false;
    if (const char *str = request.GetQueryValue("bundle"); str) {
        if (!ParseBool(str, &bundle)) {
            io->SendError(422);
            return false;
        }
    } else {
        bundle = (fs_version > 0);
    }

    // Lookup file in database
    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT IIF(?3 = 1 AND bundle IS NOT NULL, bundle, sha256)
                                  FROM fs_index
                                  WHERE version = ?1 AND filename = ?2)",
                               &stmt, fs_version, filename, 0 + bundle))
        return true;
    if (!stmt.Step())
        return !stmt.IsValid();

    const char *sha256 = (const char *)sqlite3_column_text(stmt, 0);

    // Handle hash check
    if (client_sha256 && !TestStr(client_sha256, sha256)) {
        LogError("Fetch refused because of sha256 mismatch");
        io->SendError(422);
        return true;
    }

    int64_t max_age = (explicit_version && fs_version > 0) ? (28ll * 86400000) : 0;
    ServeFile(io, instance, sha256, filename.ptr, false, max_age);

    return true;
}

void HandleFilePut(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildCode)) {
        LogError("User is not allowed to upload files");
        io->SendError(403);
        return;
    }

    const char *url = request.path + 1 + instance->key.len;
    const char *filename = url + 7;
    const char *expect = request.GetQueryValue("sha256");
    bool bundle = false;

    if (!StartsWith(url, "/files/")) {
        LogError("Cannot write to file outside '/files/'");
        io->SendError(403);
        return;
    }
    if (!filename[0]) {
        LogError("Empty filename");
        io->SendError(422);
        return;
    }
    if (expect && !CheckSha256(expect)) {
        io->SendError(422);
        return;
    }

    if (const char *str = request.GetQueryValue("bundle"); str) {
        if (!ParseBool(str, &bundle)) {
            io->SendError(422);
            return;
        }
    }

    CompressionType compression_type = CanCompressFile(filename) ? CompressionType::Gzip
                                                                 : CompressionType::None;
    const char *sha256 = nullptr;

    if (!PutFile(io, instance, compression_type, expect, &sha256))
        return;

    if (bundle) {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(UPDATE fs_index SET bundle = ?2
                                      WHERE version = 0 AND filename = ?1
                                      RETURNING version)",
                                   &stmt, filename, sha256))
            return;
        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Cannot upload bundle for '%1'", filename);
                io->SendError(404);
            }
            return;
        }
    } else {
        if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                  VALUES (0, ?1, ?2)
                                  ON CONFLICT DO UPDATE SET sha256 = excluded.sha256,
                                                            bundle = NULL)",
                               filename, sha256))
            return;
    }

    io->SendText(200, "{}", "application/json");
}

void HandleFileDelete(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildCode)) {
        LogError("User is not allowed to delete files");
        io->SendError(403);
        return;
    }

    const char *url = request.path + 1 + instance->key.len;
    const char *filename = url + 7;
    const char *client_sha256 = request.GetQueryValue("sha256");

    if (!StartsWith(url, "/files/")) {
        LogError("Cannot write to file outside '/files/'");
        io->SendError(403);
        return;
    }
    if (!filename[0]) {
        LogError("Empty filename");
        io->SendError(422);
        return;
    }
    if (client_sha256 && !CheckSha256(client_sha256)) {
        io->SendError(422);
        return;
    }

    instance->db->Transaction([&]() {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(DELETE FROM fs_index
                                      WHERE version = 0 AND filename = ?1
                                      RETURNING sha256)", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            if (client_sha256) {
                const char *sha256 = (const char *)sqlite3_column_text(stmt, 0);

                if (!TestStr(sha256, client_sha256)) {
                    LogError("Deletion refused because of sha256 mismatch");
                    io->SendError(422);
                    return false;
                }
            }

            io->SendText(200, "{}", "application/json");
            return true;
        } else if (stmt.IsValid()) {
            io->SendError(404);
            return false;
        } else {
            return false;
        }
    });
}

void HandleFileHistory(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildCode)) {
        LogError("User is not allowed to consult file history");
        io->SendError(403);
        return;
    }

    const char *filename = request.GetQueryValue("filename");
    if (!filename) {
        LogError("Missing 'filename' parameter");
        io->SendError(422);
        return;
    }

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT v.version, v.mtime, i.sha256 FROM fs_index i
                                  INNER JOIN fs_versions v ON (v.version = i.version)
                                  WHERE i.filename = ?1 ORDER BY i.version)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("File '%1' does not exist", filename);
            io->SendError(404);
        }
        return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        do {
            json->StartObject();
            json->Key("version"); json->Int64(sqlite3_column_int64(stmt, 0));
            json->Key("mtime"); json->Int64(sqlite3_column_int64(stmt, 1));
            json->Key("sha256"); json->String((const char *)sqlite3_column_text(stmt, 2));
            json->EndObject();
        } while (stmt.Step());
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

void HandleFileRestore(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildCode)) {
        LogError("User is not allowed to restore file");
        io->SendError(403);
        return;
    }

    PublishFile file = {};
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "filename") {
                    json->ParseString(&file.filename);
                } else if (key == "sha256") {
                    json->ParseString(&file.sha256);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!file.filename || !file.filename[0]) {
                    LogError("Missing or empty 'filename' parameter");
                    valid = false;
                }
                if (PathContainsDotDot(file.filename)) {
                    LogError("File name must not contain any '..' component");
                    valid = false;
                }

                if (file.sha256 && file.sha256[0]) {
                    valid &= CheckSha256(file.sha256);
                } else {
                    LogError("Missing or empty file sha256");
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

    if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                              VALUES (0, ?1, ?2)
                              ON CONFLICT DO UPDATE SET sha256 = excluded.sha256)",
                           file.filename, file.sha256))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleFileDelta(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildCode)) {
        LogError("User is not allowed to publish a new version");
        io->SendError(403);
        return;
    }

    int64_t from_version = 0;
    int64_t to_version = 0;
    if (request.GetQueryValue("from") || request.GetQueryValue("to")) {
        bool valid = true;

        if (const char *str = request.GetQueryValue("from"); str) {
            valid &= ParseInt(str, &from_version);
        } else {
            LogError("Missing 'from' parameter");
            valid = false;
        }

        if (const char *str = request.GetQueryValue("to"); str) {
            valid &= ParseInt(str, &to_version);
        } else {
            LogError("Missing 'to' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    } else {
        from_version = instance->fs_version.load();
        to_version = 0;
    }

    sq_Statement stmt1;
    sq_Statement stmt2;
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256, i.bundle
                                  FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1
                                  ORDER BY i.filename)", &stmt1))
        return;
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256, i.bundle
                                  FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1
                                  ORDER BY i.filename)", &stmt2))
        return;
    sqlite3_bind_int64(stmt1, 1, from_version);
    sqlite3_bind_int64(stmt2, 1, to_version);
    if (!stmt1.Run())
        return;
    if (!stmt2.Run())
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt1.IsRow() || stmt2.IsRow()) {
            const char *from = stmt1.IsRow() ? (const char *)sqlite3_column_text(stmt1, 0) : nullptr;
            const char *to = stmt2.IsRow() ? (const char *)sqlite3_column_text(stmt2, 0) : nullptr;

            int cmp = (from && to) ? CmpStr(from, to) : (!from - !to);
            K_ASSERT(from || to);

            json->StartObject();

            json->Key("filename"); json->String(cmp < 0 ? from : to);

            if (cmp <= 0) {
                const char *bundle = (const char *)sqlite3_column_text(stmt1, 3);

                json->Key("from"); json->StartObject();
                json->Key("size"); json->Int64(sqlite3_column_int64(stmt1, 1));
                json->Key("sha256"); json->String((const char *)sqlite3_column_text(stmt1, 2));
                if (bundle) {
                    json->Key("bundle"); json->String(bundle);
                } else {
                    json->Key("bundle"); json->Null();
                }
                json->EndObject();

                stmt1.Run();
            }

            if (cmp >= 0) {
                const char *bundle = (const char *)sqlite3_column_text(stmt2, 3);

                json->Key("to"); json->StartObject();
                json->Key("size"); json->Int64(sqlite3_column_int64(stmt2, 1));
                json->Key("sha256"); json->String((const char *)sqlite3_column_text(stmt2, 2));
                if (bundle) {
                    json->Key("bundle"); json->String(bundle);
                } else {
                    json->Key("bundle"); json->Null();
                }
                json->EndObject();

                stmt2.Run();
            }

            json->EndObject();
        }

        json->EndArray();
    });
}

void HandleFilePublish(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::BuildPublish)) {
        LogError("User is not allowed to publish a new version");
        io->SendError(403);
        return;
    }

    HashTable<const char *, PublishFile> files;
    {
        bool success = http_ParseJson(io, Mebibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                PublishFile file = {};

                json->ParseKey(&file.filename);

                switch (json->PeekToken()) {
                    case json_TokenType::String: { json->ParseString(&file.sha256); } break;

                    case json_TokenType::StartObject: {
                        for (json->ParseObject(); json->InObject(); ) {
                            Span<const char> key = json->ParseKey();

                            if (key == "sha256") {
                                json->ParseString(&file.sha256);
                            } else if (key == "bundle") {
                                json->SkipNull() || json->ParseString(&file.bundle);
                            } else {
                                json->UnexpectedKey(key);
                                valid = false;
                            }
                        }
                    } break;

                    default: {
                        LogError("Unexpected value type for file reference");
                        valid = false;
                    } break;
                }

                bool inserted;
                files.TrySet(file, &inserted);

                if (!inserted) {
                    LogError("Duplicate file '%1'", file.filename);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                for (const PublishFile &file: files) {
                    if (!file.filename || !file.filename[0]) {
                        LogError("Missing or empty file name");
                        valid = false;
                    }
                    if (PathContainsDotDot(file.filename)) {
                        LogError("File name must not contain any '..' component");
                        valid = false;
                    }

                    if (file.sha256 && file.sha256[0]) {
                        valid &= CheckSha256(file.sha256);
                    } else {
                        LogError("Missing or empty file sha256");
                        valid = false;
                    }

                    if (file.bundle) {
                        if (file.bundle[0]) {
                            valid &= CheckSha256(file.bundle);
                        } else {
                            LogError("Empty file bundle");
                            valid = false;
                        }
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

    int64_t version = -1;

    bool success = instance->db->Transaction([&]() {
        int64_t mtime = GetUnixTime();

        // Create new version
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(INSERT INTO fs_versions (mtime, userid, username, atomic)
                                          VALUES (?1, ?2, ?3, 1)
                                          RETURNING version)",
                                       &stmt, mtime, session->userid, session->username))
                return false;
            if (!stmt.GetSingleValue(&version))
                return false;
        }

        for (const PublishFile &file: files) {
            if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256, bundle)
                                      VALUES (?1, ?2, ?3, ?4))",
                                   version, file.filename, file.sha256, file.bundle)) {
                if (sqlite3_extended_errcode(*instance->db) == SQLITE_CONSTRAINT_FOREIGNKEY) {
                    LogError("Object '%1' does not exist", file.sha256);
                    io->SendError(404);
                }

                return false;
            }
        }

        // Copy to test version
        if (!instance->db->Run(R"(UPDATE fs_versions SET mtime = ?1, userid = ?2, username = ?3
                                  WHERE version = 0)",
                               mtime, session->userid, session->username))
            return false;
        if (!instance->db->Run(R"(DELETE FROM fs_index WHERE version = 0)"))
            return false;
        if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                      SELECT 0, filename, sha256 FROM fs_index WHERE version = ?1)", version))
            return false;

        if (!instance->db->Run("UPDATE fs_settings SET value = ?1 WHERE key = 'FsVersion'", version))
            return false;

        const char *json = Fmt(io->Allocator(), "{\"version\": %1}", version).ptr;
        io->SendText(200, json, "application/json");

        return true;
    });
    if (!success)
        return;

    K_ASSERT(version >= 0);
    if (!instance->SyncViews(gp_config.view_directory))
        return;
    instance->fs_version = version;
}

}
