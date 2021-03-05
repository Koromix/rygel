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
#include "user.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

void HandleFileList(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance != instance->master) {
        LogError("Cannot list files through slave instance");
        io->AttachError(403);
        return;
    }

    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT filename, size, sha256 FROM fs_files
                                 WHERE active = 1
                                 ORDER BY filename)", &stmt))
        return;

    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        json.StartObject();
        json.Key("filename"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("size"); json.Int64(sqlite3_column_int64(stmt, 1));
        json.Key("sha256"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

// Returns true when request has been handled (file exists or an error has occured)
bool HandleFileGet(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    const char *url = request.url + instance->key.len + 1;
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
    if (instance != instance->master) {
        LogError("Cannot get files through slave instance");
        io->AttachError(403);
        return true;
    }

    const char *filename = url + 7;

    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT rowid, compression, sha256 FROM fs_files
                                 WHERE active = 1 AND filename = ?1)", &stmt))
        return true;
    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

    // File does not exist, or an error has occured
    if (!stmt.Next())
        return !stmt.IsValid();

    int64_t rowid = sqlite3_column_int64(stmt, 0);
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

    // Mime type
    {
        const char *mime_type = http_GetMimeType(GetPathExtension(filename), nullptr);
        if (mime_type) {
            io->AddHeader("Content-Type", mime_type);
        }
    }

    CompressionType compression_type;
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (!name || !OptionToEnum(CompressionTypeNames, name, &compression_type)) {
            LogError("Unknown compression type '%1'", name);
            return true;
        }
    }

    sqlite3_blob *blob;
    Size blob_len;
    if (sqlite3_blob_open(instance->db, "main", "fs_files", "blob", rowid, 0, &blob) != SQLITE_OK) {
        LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
        return true;
    }
    blob_len = sqlite3_blob_bytes(blob);

    // SQLite data needs to remain valid until the end of connection
    io->AddFinalizer([blob, stmt = stmt.Leak()]() {
        sqlite3_blob_close(blob);
        sqlite3_finalize(stmt);
    });

    io->RunAsync([=]() {
        io->AddCachingHeaders(0, sha256);

        if (request.compression_type == compression_type && blob_len <= 65536) {
            StreamWriter writer;
            if (!io->OpenForWrite(200, CompressionType::None, &writer))
                return;
            io->AddEncodingHeader(compression_type);

            LocalArray<char, 65536> buf;
            if (sqlite3_blob_read(blob, buf.data, (int)blob_len, 0) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                return;
            }
            buf.len = blob_len;

            // Not much we can do at this stage in case of error. Client will get truncated data.
            writer.Write(buf);
            writer.Close();
        } else {
            StreamWriter writer;
            if (!io->OpenForWrite(200, request.compression_type, &writer))
                return;
            io->AddEncodingHeader(request.compression_type);

            Size offset = 0;
            StreamReader reader([&](Span<uint8_t> buf) {
                Size copy_len = std::min(blob_len - offset, buf.len);

                if (sqlite3_blob_read(blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                    return (Size)-1;
                }

                offset += copy_len;
                return copy_len;
            }, filename, compression_type);

            // Not much we can do at this stage in case of error. Client will get truncated data.
            SpliceStream(&reader, -1, &writer);
            writer.Close();
        }
    });

    return true;
}

void HandleFilePut(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }

    const char *url = request.url + instance->key.len + 1;
    const char *client_sha256 = request.GetQueryValue("sha256");

    const InstanceToken *token = session->GetToken(instance);
    if (!token || !token->HasPermission(UserPermission::Deploy)) {
        LogError("User is not allowed to deploy changes");
        io->AttachError(403);
        return;
    }
    if (!StartsWith(url, "/files/")) {
        LogError("Cannot write to file outside '/files/'");
        io->AttachError(403);
        return;
    }
    if (PathContainsDotDot(url)) {
        LogError("Path must not contain any '..' component");
        io->AttachError(403);
        return;
    }
    if (instance != instance->master) {
        LogError("Cannot save files through slave instance");
        io->AttachError(403);
        return;
    }

    const char *filename = url + 7;

    io->RunAsync([=]() {
        // Create temporary file
        FILE *fp = nullptr;
        const char *tmp_filename = CreateTemporaryFile(gp_domain.config.temp_directory, "", ".tmp",
                                                       &io->allocator, &fp);
        if (!tmp_filename)
            return;
        RG_DEFER {
            fclose(fp);
            UnlinkFile(tmp_filename);
        };

        // Read and compress request body
        Size total_len = 0;
        char sha256[65];
        {
            StreamWriter writer(fp, "<temp>", CompressionType::Gzip);
            StreamReader reader;
            if (!io->OpenForRead(instance->config.max_file_size, &reader))
                return;

            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);

            while (!reader.IsEOF()) {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return;
                total_len += buf.len;

                if (!writer.Write(buf))
                    return;

                crypto_hash_sha256_update(&state, buf.data, buf.len);
            }
            if (!writer.Close())
                return;

            uint8_t hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256_final(&state, hash);
            FormatSha256(hash, sha256);
        }

        // Copy and commit to database
        instance->db.Transaction([&]() {
            Size file_len = ftell(fp);
            if (fseek(fp, 0, SEEK_SET) < 0) {
                LogError("fseek('<temp>') failed: %1", strerror(errno));
                return false;
            }

            if (client_sha256) {
                sq_Statement stmt;
                if (!instance->db.Prepare(R"(SELECT sha256 FROM fs_files
                                             WHERE active = 1 AND filename = ?1)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

                if (stmt.Next()) {
                    const char *sha256 = (const char *)sqlite3_column_text(stmt, 0);

                    if (!TestStr(client_sha256, sha256)) {
                        LogError("Update refused because of sha256 mismatch");
                        io->AttachError(409);
                        return false;
                    }
                } else if (stmt.IsValid()) {
                    if (client_sha256[0]) {
                        LogError("Update refused because of sha256 mismatch");
                        io->AttachError(409);
                        return false;
                    }
                } else {
                    return false;
                }
            }

            int64_t mtime = GetUnixTime();

            if (!instance->db.Run(R"(UPDATE fs_files SET active = 0
                                     WHERE active = 1 AND filename = ?1)", filename))
                return false;
            if (!instance->db.Run(R"(INSERT INTO fs_files (active, filename, mtime, blob, compression, sha256, size)
                                     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7))",
                                  1, filename, mtime, sq_Binding::Zeroblob(file_len), "Gzip", sha256, total_len))
                return false;

            int64_t rowid = sqlite3_last_insert_rowid(instance->db);

            sqlite3_blob *blob;
            if (sqlite3_blob_open(instance->db, "main", "fs_files", "blob", rowid, 1, &blob) != SQLITE_OK)
                return false;
            RG_DEFER { sqlite3_blob_close(blob); };

            StreamReader reader(fp, "<temp>");
            Size read_len = 0;

            while (!reader.IsEOF()) {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return false;

                if (buf.len + read_len > file_len) {
                    LogError("Temporary file size has changed (bigger)");
                    return false;
                }
                if (sqlite3_blob_write(blob, buf.data, (int)buf.len, (int)read_len) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                    return false;
                }

                read_len += buf.len;
            }
            if (read_len < file_len) {
                LogError("Temporary file size has changed (truncated)");
                return false;
            }

            io->AttachText(200, "Done!");
            return true;
        });
    });
}

void HandleFileDelete(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }

    const char *url = request.url + instance->key.len + 1;
    const char *client_sha256 = request.GetQueryValue("sha256");

    const InstanceToken *token = session->GetToken(instance);
    if (!token || !token->HasPermission(UserPermission::Deploy)) {
        LogError("User is not allowed to deploy changes");
        io->AttachError(403);
        return;
    }
    if (!StartsWith(url, "/files/")) {
        LogError("Cannot delete files outside '/files/'");
        io->AttachError(403);
        return;
    }
    if (instance != instance->master) {
        LogError("Cannot delete files through slave instance");
        io->AttachError(403);
        return;
    }

    const char *filename = url + 7;

    instance->db.Transaction([&]() {
        if (client_sha256) {
            sq_Statement stmt;
            if (!instance->db.Prepare(R"(SELECT active, sha256 FROM fs_files
                                         WHERE filename = ?1
                                         ORDER BY active DESC)", &stmt))
                return false;
            sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

            if (stmt.Next()) {
                bool active = sqlite3_column_int(stmt, 0);
                const char *sha256 = active ? (const char *)sqlite3_column_text(stmt, 1) : "";

                if (!TestStr(client_sha256, sha256)) {
                    LogError("Deletion refused because of sha256 mismatch");
                    io->AttachError(409);
                    return false;
                }
            } else {
                if (stmt.IsValid() && client_sha256[0]) {
                    LogError("Deletion refused because of sha256 mismatch");
                    io->AttachError(409);
                }

                return false;
            }
        }

        if (!instance->db.Run(R"(UPDATE fs_files SET active = 0
                                 WHERE active = 1 AND filename = ?1)", filename))
            return false;

        if (sqlite3_changes(instance->db)) {
            io->AttachText(200, "Done!");
        } else {
            io->AttachError(404);
        }
        return true;
    });
}

}
