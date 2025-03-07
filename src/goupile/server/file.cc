// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "domain.hh"
#include "file.hh"
#include "instance.hh"
#include "goupile.hh"
#include "user.hh"

#if defined(_WIN32)
    #include <io.h>
#endif

namespace RG {

void HandleFileList(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

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
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256 FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1
                                  ORDER BY i.filename)", &stmt))
        return;
    sqlite3_bind_int64(stmt, 1, fs_version);

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    json.Key("version"); json.Int64(fs_version);
    json.Key("files"); json.StartArray();
    while (stmt.Step()) {
        json.StartObject();
        json.Key("filename"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("size"); json.Int64(sqlite3_column_int64(stmt, 1));
        json.Key("sha256"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();
    json.EndObject();

    json.Finish();
}

static void AddMimeTypeHeader(http_IO *io, const char *filename)
{
   const char *mimetype = GetMimeType(GetPathExtension(filename), nullptr);

    if (mimetype) {
        io->AddHeader("Content-Type", mimetype);
    }
}

// Returns true when request has been handled (file exists or an error has occured)
bool HandleFileGet(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    const char *url = request.path + 1 + instance->key.len;

    RG_ASSERT(url <= request.path + strlen(request.path));
    RG_ASSERT(url[0] == '/');

    const char *client_etag = request.GetHeaderValue("If-None-Match");
    const char *client_sha256 = request.GetQueryValue("sha256");

    // Handle special paths
    if (TestStr(url, "/favicon.png")) {
        url ="/files/favicon.png";
    } else if (TestStr(url, "/manifest.json")) {
        url = "/files/manifest.json";
    }

    // Safety checks
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

    // Lookup file in database
    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT o.rowid, o.compression, o.sha256 FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1 AND i.filename = ?2)", &stmt))
        return true;
    sqlite3_bind_int64(stmt, 1, fs_version);
    sqlite3_bind_text(stmt, 2, filename.ptr, (int)filename.len, SQLITE_STATIC);
    if (!stmt.Step())
        return !stmt.IsValid();

    int64_t rowid = sqlite3_column_int64(stmt, 0);

    // Handle hash check and caching
    {
        const char *sha256 = (const char *)sqlite3_column_text(stmt, 2);

        if (client_etag && TestStr(client_etag, sha256)) {
            io->SendEmpty(304);
            return true;
        }
        if (client_sha256 && !TestStr(client_sha256, sha256)) {
            LogError("Fetch refused because of sha256 mismatch");
            io->SendError(422);
            return true;
        }

        int64_t max_age = (explicit_version && fs_version > 0) ? (365ll * 86400000) : 0;
        io->AddCachingHeaders(max_age, sha256);
    }

    // Negociate content encoding
    CompressionType src_encoding;
    CompressionType dest_encoding;
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (!name || !OptionToEnumI(CompressionTypeNames, name, &src_encoding)) {
            LogError("Unknown compression type '%1'", name);
            return true;
        }

        if (!io->NegociateEncoding(src_encoding, &dest_encoding))
            return true;
    }

    // Open file blob
    sqlite3_blob *src_blob;
    Size src_len;
    if (sqlite3_blob_open(*instance->db, "main", "fs_objects", "blob", rowid, 0, &src_blob) != SQLITE_OK) {
        LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
        return true;
    }
    src_len = sqlite3_blob_bytes(src_blob);
    RG_DEFER { sqlite3_blob_close(src_blob); };

    // Fast path (small objects)
    if (dest_encoding == src_encoding && src_len <= 65536) {

        uint8_t *ptr = (uint8_t *)AllocateRaw(io->Allocator(), src_len);

        if (sqlite3_blob_read(src_blob, ptr, (int)src_len, 0) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
            return true;
        }

        io->AddEncodingHeader(dest_encoding);
        AddMimeTypeHeader(io, filename.ptr);

        io->SendBinary(200, MakeSpan(ptr, src_len));

        return true;
    }

    // Handle range requests
    if (src_encoding == CompressionType::None && dest_encoding == src_encoding) {
        LocalArray<http_ByteRange, 16> ranges;
        {
            const char *str = request.GetHeaderValue("Range");

            if (str && !http_ParseRange(str, src_len, &ranges)) {
                io->SendError(416);
                return true;
            }
        }

        if (ranges.len >= 2) {
            char boundary[17];
            {
                uint64_t buf;
                FillRandomSafe(&buf, RG_SIZE(buf));
                Fmt(boundary, "%1", FmtHex(buf).Pad0(-16));
            }

            // Boundary strings
            LocalArray<Span<const char>, RG_LEN(ranges.data) * 2> boundaries;
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
                return true;

            for (Size i = 0; i < ranges.len; i++) {
                const http_ByteRange &range = ranges[i];
                Size range_len = range.end - range.start;

                writer.Write(boundaries[i * 2]);

                Size offset = 0;
                while (offset < range_len) {
                    uint8_t buf[16384];
                    Size copy_len = std::min(range_len - offset, RG_SIZE(buf));

                    if (sqlite3_blob_read(src_blob, buf, (int)copy_len, (int)(range.start + offset)) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                        return true;
                    }

                    writer.Write(buf, copy_len);
                    offset += copy_len;
                }

                writer.Write(boundaries[i * 2 + 1]);
            }
            writer.Close();

            return true;
        } else if (ranges.len == 1) {
            const http_ByteRange &range = ranges[0];
            Size range_len = range.end - range.start;

            // Add headers
            {
                char buf[512];
                Fmt(buf, "bytes %1-%2/%3", range.start, range.end - 1, src_len);

                io->AddHeader("Content-Range", buf);
                io->AddEncodingHeader(dest_encoding);
                AddMimeTypeHeader(io, filename.ptr);
            }

            StreamWriter writer;
            if (!io->OpenForWrite(206, dest_encoding, range_len, &writer))
                return  true;

            Size offset = 0;
            while (offset < range_len) {
                uint8_t buf[16384];
                Size copy_len = std::min(range_len - offset, RG_SIZE(buf));

                if (sqlite3_blob_read(src_blob, buf, (int)copy_len, (int)(range.start + offset)) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                    return true;
                }

                writer.Write(buf, copy_len);
                offset += copy_len;
            }
            writer.Close();

            return true;
        } else {
            io->AddHeader("Accept-Ranges", "bytes");

            // Go on with default code path
        }
    }

    // Default path, for big files and/or transcoding (Gzip to None, etc.)
    {
        io->AddEncodingHeader(dest_encoding);
        AddMimeTypeHeader(io, filename.ptr);

        StreamWriter writer;
        if (src_encoding == dest_encoding) {
            src_encoding = CompressionType::None;

            if (!io->OpenForWrite(200, src_len, &writer))
                return true;
        } else {
            if (!io->OpenForWrite(200, dest_encoding, -1, &writer))
                return true;
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
        }, filename.ptr, src_encoding);

        // Not much we can do at this stage in case of error. Client will get truncated data.
        SpliceStream(&reader, -1, &writer);
        writer.Close();
    }

    return true;
}

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

    // See if this object is already on the server
    if (client_sha256) {
        sq_Statement stmt;
        if (!instance->db->Prepare("SELECT rowid FROM fs_objects WHERE sha256 = ?1", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, client_sha256, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                      VALUES (0, ?1, ?2)
                                      ON CONFLICT DO UPDATE SET sha256 = excluded.sha256)",
                                   filename, client_sha256))
                return;

            io->SendText(200, "{}", "application/json");
            return;
        } else if (!stmt.IsValid()) {
            return;
        }
    }

    // Create temporary file
    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(gp_domain.config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
    if (!tmp_filename)
        return;
    RG_DEFER {
        CloseDescriptor(fd);
        UnlinkFile(tmp_filename);
    };

    CompressionType compression_type = CanCompressFile(filename) ? CompressionType::Gzip
                                                                    : CompressionType::None;

    // Read and compress request body
    int64_t total_len = 0;
    char sha256[65];
    {
        StreamWriter writer(fd, "<temp>", 0, compression_type);
        StreamReader reader;
        if (!io->OpenForRead(instance->config.max_file_size, &reader))
            return;

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);
            if (buf.len < 0)
                return;
            total_len += buf.len;

            if (!writer.Write(buf))
                return;

            crypto_hash_sha256_update(&state, buf.data, buf.len);
        } while (!reader.IsEOF());
        if (!writer.Close())
            return;

        uint8_t hash[crypto_hash_sha256_BYTES];
        crypto_hash_sha256_final(&state, hash);
        FormatSha256(hash, sha256);
    }

    // Don't lie to me :)
    if (client_sha256 && !TestStr(sha256, client_sha256)) {
        LogError("Upload refused because of sha256 mismatch");
        io->SendError(422);
        return;
    }

    // Copy and commit to database
    instance->db->Transaction([&]() {
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
        RG_DEFER { sqlite3_blob_close(blob); };

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

        if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                  VALUES (0, ?1, ?2)
                                  ON CONFLICT DO UPDATE SET sha256 = excluded.sha256)",
                               filename, sha256))
            return false;

        io->SendText(200, "{}", "application/json");
        return true;
    });
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

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    do {
        json.StartObject();
        json.Key("version"); json.Int64(sqlite3_column_int64(stmt, 0));
        json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 1));
        json.Key("sha256"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.EndObject();
    } while (stmt.Step());
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
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

    const char *filename = nullptr;
    const char *sha256 = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Megabytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "filename") {
                parser.ParseString(&filename);
            } else if (key == "sha256") {
                parser.ParseString(&sha256);
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
            io->SendError(422);
            return;
        }
    }

    // Check missing or invalid values
    {
        bool valid = true;

        if (!filename || !filename[0]) {
            LogError("Missing or empty 'filename' value");
            valid = false;
        }
        if (!sha256 || !sha256[0]) {
            LogError("Missing or empty 'sha256' value");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                              VALUES (0, ?1, ?2)
                              ON CONFLICT DO UPDATE SET sha256 = excluded.sha256)",
                           filename, sha256))
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
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256 FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1 ORDER BY i.filename)", &stmt1))
        return;
    if (!instance->db->Prepare(R"(SELECT i.filename, o.size, i.sha256 FROM fs_index i
                                  INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                  WHERE i.version = ?1 ORDER BY i.filename)", &stmt2))
        return;
    sqlite3_bind_int64(stmt1, 1, from_version);
    sqlite3_bind_int64(stmt2, 1, to_version);
    if (!stmt1.Run())
        return;
    if (!stmt2.Run())
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    while (stmt1.IsRow() || stmt2.IsRow()) {
        const char *from = stmt1.IsRow() ? (const char *)sqlite3_column_text(stmt1, 0) : nullptr;
        const char *to = stmt2.IsRow() ? (const char *)sqlite3_column_text(stmt2, 0) : nullptr;

        json.StartObject();
        if (!from) {
            json.Key("filename"); json.String(to);
            json.Key("to_size"); json.Int64(sqlite3_column_int64(stmt2, 1));
            json.Key("to_sha256"); json.String((const char *)sqlite3_column_text(stmt2, 2));

            stmt2.Run();
        } else if (!to) {
            json.Key("filename"); json.String(from);
            json.Key("from_size"); json.Int64(sqlite3_column_int64(stmt1, 1));
            json.Key("from_sha256"); json.String((const char *)sqlite3_column_text(stmt1, 2));

            stmt1.Run();
        } else {
            int cmp = CmpStr(from, to);

            if (cmp < 0) {
                json.Key("filename"); json.String(from);
                json.Key("from_size"); json.Int64(sqlite3_column_int64(stmt1, 1));
                json.Key("from_sha256"); json.String((const char *)sqlite3_column_text(stmt1, 2));

                stmt1.Run();
            } else if (cmp > 0) {
                json.Key("filename"); json.String(to);
                json.Key("to_size"); json.Int64(sqlite3_column_int64(stmt2, 1));
                json.Key("to_sha256"); json.String((const char *)sqlite3_column_text(stmt2, 2));

                stmt2.Run();
            } else {
                json.Key("filename"); json.String(from);
                json.Key("from_size"); json.Int64(sqlite3_column_int64(stmt1, 1));
                json.Key("from_sha256"); json.String((const char *)sqlite3_column_text(stmt1, 2));
                json.Key("to_size"); json.Int64(sqlite3_column_int64(stmt2, 1));
                json.Key("to_sha256"); json.String((const char *)sqlite3_column_text(stmt2, 2));

                stmt1.Run();
                stmt2.Run();
            }
        }
        json.EndObject();
    }
    json.EndArray();

    json.Finish();
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

    HashMap<const char *, const char *> files;
    {
        StreamReader st;
        if (!io->OpenForRead(Megabytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            const char *filename = "";
            const char *sha256 = "";

            parser.ParseKey(&filename);
            parser.ParseString(&sha256);

            bool inserted;
            files.TrySet(filename, sha256, &inserted);

            if (!inserted) {
                LogError("Duplicate file '%1'", filename);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
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

        for (const auto &file: files.table) {
            if (!file.key[0]) {
                LogError("Empty filenames are not allowed");
                io->SendError(403);
                return false;
            }
            if (PathContainsDotDot(file.key)) {
                LogError("File name must not contain any '..' component");
                io->SendError(403);
                return false;
            }
            if (!CheckSha256(file.value)) {
                io->SendError(422);
                return false;
            }

            if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                      VALUES (?1, ?2, ?3))",
                                   version, file.key, file.value)) {
                if (sqlite3_extended_errcode(*instance->db) == SQLITE_CONSTRAINT_FOREIGNKEY) {
                    LogError("Object '%1' does not exist", file.value);
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

    RG_ASSERT(version >= 0);
    if (!instance->SyncViews(gp_domain.config.view_directory))
        return;
    instance->fs_version = version;
}

}
