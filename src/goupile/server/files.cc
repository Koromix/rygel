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

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "files.hh"
#include "instance.hh"
#include "goupile.hh"
#include "session.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

void HandleFileList(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->master != instance) {
        LogError("Cannot list files through slave instance");
        io->AttachError(403);
        return;
    }

    int64_t fs_version;
    if (const char *str = request.GetQueryValue("version"); str) {
        if (!ParseInt(str, &fs_version)) {
            io->AttachError(422);
            return;
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

static void AddMimeTypeHeader(const char *filename, http_IO *io)
{
   const char *mime_type = http_GetMimeType(GetPathExtension(filename), nullptr);

    if (mime_type) {
        io->AddHeader("Content-Type", mime_type);
    }
}

// Returns true when request has been handled (file exists or an error has occured)
bool HandleFileGet(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    const char *url = request.url + 1 + instance->key.len;
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
        io->AttachError(403);
        return true;
    }

    Span<const char> filename = url + 7;

    int64_t fs_version;
    bool explicit_version;
    {
        Span<const char> remain;

        if (ParseInt(filename, &fs_version, 0, &remain) && remain.ptr[0] == '/') {
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
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            io->AttachResponse(304, response);
            return true;
        }
        if (client_sha256 && !TestStr(client_sha256, sha256)) {
            LogError("Fetch refused because of sha256 mismatch");
            io->AttachError(409);
            return true;
        }

        io->AddCachingHeaders(explicit_version ? (365 * 86400) : 0, sha256);
    }

    // Negociate content encoding
    CompressionType src_encoding;
    CompressionType dest_encoding;
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (!name || !OptionToEnum(CompressionTypeNames, name, &src_encoding)) {
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

    // Fast path (small objects)
    if (dest_encoding == src_encoding && src_len <= 65536) {
        RG_DEFER { sqlite3_blob_close(src_blob); };

        uint8_t *ptr = (uint8_t *)Allocator::Allocate(&io->allocator, src_len);
        io->AddFinalizer([=] { Allocator::Release(&io->allocator, ptr, src_len); });

        if (sqlite3_blob_read(src_blob, ptr, (int)src_len, 0) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
            return true;
        }

        MHD_Response *response =
            MHD_create_response_from_buffer((size_t)src_len, (void *)ptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(200, response);
        io->AddEncodingHeader(dest_encoding);
        AddMimeTypeHeader(filename.ptr, io);

        return true;
    }

    // The blob needs to remain valid until the end of connection
    io->AddFinalizer([=]() { sqlite3_blob_close(src_blob); });

    io->RunAsync([=]() mutable {
        // Handle range requests
        if (src_encoding == CompressionType::None && dest_encoding == src_encoding) {
            LocalArray<http_ByteRange, 16> ranges;
            {
                const char *str = request.GetHeaderValue("Range");

                if (str && !http_ParseRange(str, src_len, &ranges)) {
                    io->AttachError(416);
                    return;
                }
            }

            if (ranges.len >= 2) {
                char boundary[17];
                {
                    uint64_t buf;
                    randombytes_buf(&buf, RG_SIZE(buf));
                    Fmt(boundary, "%1", FmtHex(buf).Pad0(-16));
                }

                // Boundary strings
                LocalArray<Span<const char>, RG_LEN(ranges.data) * 2> boundaries;
                Size total_len = 0;
                {
                    const char *mime_type = http_GetMimeType(GetPathExtension(filename), nullptr);

                    for (Size i = 0; i < ranges.len; i++) {
                        const http_ByteRange &range = ranges[i];

                        Span<const char> before;
                        if (mime_type) {
                            before = Fmt(&io->allocator, "Content-Type: %1\r\n"
                                                         "Content-Range: bytes %2-%3/%4\r\n\r\n",
                                         mime_type, range.start, range.end - 1, src_len);
                        } else {
                            before = Fmt(&io->allocator, "Content-Range: bytes %1-%2/%3\r\n\r\n",
                                         range.start, range.end - 1, src_len);
                        }

                        Span<const char> after;
                        if (i < ranges.len - 1) {
                            after = Fmt(&io->allocator, "\r\n--%1\r\n", boundary);
                        } else {
                            after = Fmt(&io->allocator, "\r\n--%1--\r\n", boundary);
                        }

                        boundaries.Append(before);
                        boundaries.Append(after);

                        total_len += before.len;
                        total_len += range.end - range.start;
                        total_len += after.len;
                    }
                }

                StreamWriter writer;
                if (!io->OpenForWrite(206, total_len, dest_encoding, &writer))
                    return;
                io->AddEncodingHeader(dest_encoding);

                // Range header
                {
                    char buf[512];
                    io->AddHeader("Content-Type", Fmt(buf, "multipart/byteranges; boundary=%1", boundary).ptr);
                }

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
                            return;
                        }

                        writer.Write(buf, copy_len);
                        offset += copy_len;
                    }

                    writer.Write(boundaries[i * 2 + 1]);
                }
                writer.Close();

                return;
            } else if (ranges.len == 1) {
                const http_ByteRange &range = ranges[0];
                Size range_len = range.end - range.start;

                StreamWriter writer;
                if (!io->OpenForWrite(206, range_len, dest_encoding, &writer))
                    return;
                io->AddEncodingHeader(dest_encoding);
                AddMimeTypeHeader(filename.ptr, io);

                // Range header
                {
                    char buf[512];
                    io->AddHeader("Content-Range", Fmt(buf, "bytes %1-%2/%3", range.start, range.end - 1, src_len).ptr);
                }

                Size offset = 0;
                while (offset < range_len) {
                    uint8_t buf[16384];
                    Size copy_len = std::min(range_len - offset, RG_SIZE(buf));

                    if (sqlite3_blob_read(src_blob, buf, (int)copy_len, (int)(range.start + offset)) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                        return;
                    }

                    writer.Write(buf, copy_len);
                    offset += copy_len;
                }
                writer.Close();

                return;
            } else {
                io->AddHeader("Accept-Ranges", "bytes");

                // Go on with default code path
            }
        }

        // Default path, for big files and/or transcoding (Gzip to None, etc.)
        {
            StreamWriter writer;
            if (src_encoding == dest_encoding) {
                src_encoding = CompressionType::None;

                if (!io->OpenForWrite(200, src_len, CompressionType::None, &writer))
                    return;
            } else {
                if (!io->OpenForWrite(200, -1, dest_encoding, &writer))
                    return;
            }

            io->AddEncodingHeader(dest_encoding);
            AddMimeTypeHeader(filename.ptr, io);

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
    });

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

void HandleFilePut(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::AdminPublish)) {
        LogError("User is not allowed to upload files");
        io->AttachError(403);
        return;
    }

    const char *url = request.url + 1 + instance->key.len;
    const char *client_sha256 = url + 19;
    const char *filename = request.GetQueryValue("filename");

    if (!StartsWith(url, "/api/files/objects/")) {
        LogError("Cannot write to file outside '/api/files/objects/'");
        io->AttachError(403);
        return;
    }
    if (!CheckSha256(client_sha256)) {
        io->AttachError(422);
        return;
    }

    // See if this object is already on the server
    {
        sq_Statement stmt;
        if (!instance->db->Prepare("SELECT rowid FROM fs_objects WHERE sha256 = ?1", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, client_sha256, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            LogError("This object already exists");
            io->AttachError(409);
        } else if (!stmt.IsValid()) {
            return;
        }
    }

    io->RunAsync([=]() {
        // Create temporary file
        FILE *fp = nullptr;
        const char *tmp_filename = CreateTemporaryFile(gp_domain.config.tmp_directory, "", ".tmp",
                                                       &io->allocator, &fp);
        if (!tmp_filename)
            return;
        RG_DEFER {
            fclose(fp);
            UnlinkFile(tmp_filename);
        };

        CompressionType compression_type = filename && ShouldCompressFile(filename) ? CompressionType::Gzip
                                                                                    : CompressionType::None;

        // Read and compress request body
        Size total_len = 0;
        char sha256[65];
        {
            StreamWriter writer(fp, "<temp>", compression_type);
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
        if (!TestStr(sha256, client_sha256)) {
            LogError("Upload refused because of sha256 mismatch");
            io->AttachError(403);
            return;
        }

        // Copy and commit to database
        instance->db->Transaction([&]() {
            Size file_len = ftell(fp);
            if (fseek(fp, 0, SEEK_SET) < 0) {
                LogError("fseek('<temp>') failed: %1", strerror(errno));
                return false;
            }

            int64_t mtime = GetUnixTime();

            if (!instance->db->Run(R"(INSERT INTO fs_objects (sha256, mtime, compression, size, blob)
                                      VALUES (?1, ?2, ?3, ?4, ?5))",
                                  sha256, mtime, CompressionTypeNames[(int)compression_type],
                                  total_len, sq_Binding::Zeroblob(file_len)))
                return false;

            int64_t rowid = sqlite3_last_insert_rowid(*instance->db);

            sqlite3_blob *blob;
            if (sqlite3_blob_open(*instance->db, "main", "fs_objects", "blob", rowid, 1, &blob) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(*instance->db));
                return false;
            }
            RG_DEFER { sqlite3_blob_close(blob); };

            StreamReader reader(fp, "<temp>");
            Size read_len = 0;

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

            io->AttachText(200, "Done!");
            return true;
        });
    });
}

void HandleFilePublish(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::AdminPublish)) {
        LogError("User is not allowed to publish a new version");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        HashMap<const char *, const char *> files;
        if (!io->ReadPostValues(&io->allocator, &files)) {
            io->AttachError(422);
            return;
        }

        instance->db->Transaction([&]() {
            int64_t mtime = GetUnixTime();

            if (!instance->db->Run(R"(INSERT INTO fs_versions (mtime, userid, username, atomic)
                                      VALUES (?1, ?2, ?3, 1))",
                                   mtime, session->userid, session->username))
                return false;

            int64_t version = sqlite3_last_insert_rowid(*instance->db);

            for (const auto &file: files.table) {
                if (PathContainsDotDot(file.key)) {
                    LogError("File name must not contain any '..' component");
                    io->AttachError(403);
                    return false;
                }
                if (!CheckSha256(file.value)) {
                    io->AttachError(422);
                    return false;
                }

                if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                          VALUES (?1, ?2, ?3))",
                                       version, file.key, file.value)) {
                    if (sqlite3_extended_errcode(*instance->db) == SQLITE_CONSTRAINT_FOREIGNKEY) {
                        LogError("Object '%1' does not exist", file.value);
                        io->AttachError(404);
                    }

                    return false;
                }
            }

            if (!instance->db->Run("UPDATE fs_settings SET value = ?1 WHERE key = 'FsVersion'", version))
                return false;
            instance->fs_version = version;

            const char *json = Fmt(&io->allocator, "{\"version\": %1}", version).ptr;
            io->AttachText(200, json, "application/json");

            return true;
        });
    });
}

bool ShouldCompressFile(const char *filename)
{
    char extension[8];
    {
        const char *ptr = GetPathExtension(filename).ptr;

        Size i = 0;
        while (i < RG_SIZE(extension) - 1 && ptr[i]) {
            extension[i] = LowerAscii(ptr[i]);
            i++;
        }
        extension[i] = 0;
    }

    if (TestStrI(extension, ".zip"))
        return false;
    if (TestStrI(extension, ".rar"))
        return false;
    if (TestStrI(extension, ".7z"))
        return false;
    if (TestStrI(extension, ".gz") || TestStrI(extension, ".tgz"))
        return false;
    if (TestStrI(extension, ".bz2") || TestStrI(extension, ".tbz2"))
        return false;
    if (TestStrI(extension, ".xz") || TestStrI(extension, ".txz"))
        return false;
    if (TestStrI(extension, ".zst") || TestStrI(extension, ".tzst"))
        return false;

    const char *mime_type = http_GetMimeType(extension);

    if (StartsWith(mime_type, "video/"))
        return false;
    if (StartsWith(mime_type, "audio/"))
        return false;
    if (StartsWith(mime_type, "image/"))
        return false;

    return true;
}

}
