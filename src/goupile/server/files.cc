// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "files.hh"
#include "goupile.hh"
#include "user.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include <stdio.h>

namespace RG {

void HandleFileList(const http_RequestInfo &request, http_IO *io)
{
    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT path, blob, sha256 FROM fs_files
                                 WHERE sha256 IS NOT NULL
                                 ORDER BY path;)", &stmt))
        return;

    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        json.StartObject();
        json.Key("path"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("size"); json.Int(sqlite3_column_bytes(stmt, 0));
        json.Key("sha256"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

// Returns true when request has been handled (file exists or an error has occured)
bool HandleFileGet(const http_RequestInfo &request, http_IO *io)
{
    const char *path;
    if (TestStr(request.url, "/favicon.png")) {
        path ="/files/favicon.png";
    } else if (TestStr(request.url, "/manifest.json")) {
        path = "/files/manifest.json";
    } else {
        path = request.url;
    }

    const char *client_etag = request.GetHeaderValue("If-None-Match");
    const char *client_sha256 = request.GetQueryValue("sha256");

    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT rowid, compression, sha256 FROM fs_files
                                 WHERE path = ? AND sha256 IS NOT NULL)", &stmt))
        return true;
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

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

    CompressionType compression_type;
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        bool success = OptionToEnum(CompressionTypeNames, name, &compression_type);
        RG_ASSERT(success);
    }

    sqlite3_blob *blob;
    int size;
    if (sqlite3_blob_open(instance->db, "main", "fs_files", "blob", rowid, 0, &blob) != SQLITE_OK) {
        LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
        return true;
    }
    size = sqlite3_blob_bytes(blob);

    // SQLite data needs to remain valid until the end of connection
    io->AddFinalizer([blob, stmt = stmt.Leak()]() {
        sqlite3_blob_close(blob);
        sqlite3_finalize(stmt);
    });

    io->RunAsync([=]() mutable {
        io->AddCachingHeaders(0, sha256);

        if (request.compression_type == compression_type && size <= 65536) {
            StreamWriter writer;
            if (!io->OpenForWrite(200, CompressionType::None, &writer))
                return;
            io->AddEncodingHeader(compression_type);

            LocalArray<char, 65536> buf;
            if (sqlite3_blob_read(blob, buf.data, size, 0) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                return;
            }
            buf.len = size;

            // Not much we can do at this stage in case of error. Client will get truncated data.
            writer.Write(buf);
            writer.Close();
        } else {
            StreamWriter writer;
            if (!io->OpenForWrite(200, request.compression_type, &writer))
                return;
            io->AddEncodingHeader(request.compression_type);

            int offset = 0;

            StreamReader reader([&](Span<uint8_t> buf) {
                Size copy_len = std::min((Size)size - offset, buf.len);

                if (sqlite3_blob_read(blob, buf.ptr, copy_len, offset) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                    return (Size)-1;
                }

                offset += copy_len;
                return copy_len;
            }, path, compression_type);

            // Not much we can do at this stage in case of error. Client will get truncated data.
            SpliceStream(&reader, -1, &writer);
            writer.Close();
        }
    });

    return true;
}

static void FormatSha256(Span<const uint8_t> hash, char out_sha256[65])
{
    RG_ASSERT(hash.len == 32);
    Fmt(MakeSpan(out_sha256, 65), "%1", FmtSpan(hash, FmtType::Hexadecimal, "").Pad0(-2));
}

void HandleFilePut(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->HasPermission(UserPermission::Deploy)) {
        LogError("User is not allowed to deploy changes");
        io->AttachError(403);
        return;
    }

    const char *client_sha256 = request.GetQueryValue("sha256");

    // Security checks
    if (!StartsWith(request.url, "/files/")) {
        LogError("Cannot write to file outside '/files/'");
        io->AttachError(403);
        return;
    }
    if (PathContainsDotDot(request.url)) {
        LogError("Path must not contain any '..' component");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Create temporary file
        FILE *fp = nullptr;
        const char *tmp_filename = CreateTemporaryFile(instance->config.temp_directory, ".tmp", &io->allocator, &fp);
        if (!tmp_filename)
            return;
        RG_DEFER {
            fclose(fp);
            UnlinkFile(tmp_filename);
        };

        // Read and compress request body
        char sha256[65];
        {
            StreamWriter writer(fp, "<temp>", CompressionType::Gzip);
            StreamReader reader;
            if (!io->OpenForRead(&reader))
                return;

            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);

            Size total_len = 0;

            while (!reader.IsEOF()) {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return;

                if (RG_UNLIKELY(buf.len > instance->config.max_file_size - total_len)) {
                    LogError("File '%1' is too large (limit = %2)",
                             reader.GetFileName(), FmtDiskSize(instance->config.max_file_size));
                    io->AttachError(413);
                    return;
                }
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
        sq_TransactionResult ret = instance->db.Transaction([&]() {
            // XXX: StreamX::Seek() (and Tell)
            Size file_len = ftell(fp);
            if (fseek(fp, 0, SEEK_SET) < 0) {
                LogError("fseek('<temp>') failed: %1", strerror(errno));
                return sq_TransactionResult::Error;
            }

            if (client_sha256) {
                sq_Statement stmt;
                if (!instance->db.Prepare(R"(SELECT sha256 FROM fs_files
                                             WHERE path = ? AND sha256 IS NOT NULL;)", &stmt))
                    return sq_TransactionResult::Error;
                sqlite3_bind_text(stmt, 1, request.url, -1, SQLITE_STATIC);

                if (stmt.Next()) {
                    const char *sha256 = (const char *)sqlite3_column_text(stmt, 0);

                    if (!TestStr(client_sha256, sha256)) {
                        LogError("Update refused because of sha256 mismatch");
                        io->AttachError(409);
                        return sq_TransactionResult::Rollback;
                    }
                } else if (stmt.IsValid()) {
                    if (client_sha256[0]) {
                        LogError("Update refused because of sha256 mismatch");
                        io->AttachError(409);
                        return sq_TransactionResult::Rollback;
                    }
                } else {
                    return sq_TransactionResult::Error;
                }
            }

            // We need to get the ROWID value after INSERT, but sqlite3_last_insert_rowid() does
            // not work with UPSERT. So use DELETE + INSERT instead.
            if (!instance->db.Run("DELETE FROM fs_files WHERE path = ?;", request.url))
                return sq_TransactionResult::Error;

            sq_Statement stmt;
            if (!instance->db.Prepare(R"(INSERT INTO fs_files (path, blob, compression, sha256)
                                         VALUES (?, ?, ?, ?);)", &stmt))
                return sq_TransactionResult::Error;
            sqlite3_bind_text(stmt, 1, request.url, -1, SQLITE_STATIC);
            sqlite3_bind_zeroblob(stmt, 2, (int)file_len);
            sqlite3_bind_text(stmt, 3, "Gzip", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, sha256, -1, SQLITE_STATIC);
            if (!stmt.Run())
                return sq_TransactionResult::Error;

            int64_t rowid = sqlite3_last_insert_rowid(instance->db);

            sqlite3_blob *blob;
            if (sqlite3_blob_open(instance->db, "main", "fs_files", "blob", rowid, 1, &blob) != SQLITE_OK)
                return sq_TransactionResult::Error;
            RG_DEFER { sqlite3_blob_close(blob); };

            StreamReader reader(fp, "<temp>");
            Size read_len = 0;

            while (!reader.IsEOF()) {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return sq_TransactionResult::Error;

                if (buf.len + read_len > file_len) {
                    LogError("Temporary file size has changed (bigger)");
                    return sq_TransactionResult::Error;
                }
                if (sqlite3_blob_write(blob, buf.data, (int)buf.len, (int)read_len) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
                    return sq_TransactionResult::Error;
                }

                read_len += buf.len;
            }
            if (read_len < file_len) {
                LogError("Temporary file size has changed (truncated)");
                return sq_TransactionResult::Error;
            }

            return sq_TransactionResult::Commit;
        });
        if (ret != sq_TransactionResult::Commit)
            return;

        io->AttachText(200, "Done!");
    });
}

void HandleFileDelete(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->HasPermission(UserPermission::Deploy)) {
        LogError("User is not allowed to deploy changes");
        io->AttachError(403);
        return;
    }

    const char *client_sha256 = request.GetQueryValue("sha256");

    sq_TransactionResult ret = instance->db.Transaction([&]() {
        if (client_sha256) {
            sq_Statement stmt;
            if (!instance->db.Prepare("SELECT sha256 FROM fs_files WHERE path = ?", &stmt))
                return sq_TransactionResult::Error;
            sqlite3_bind_text(stmt, 1, request.url, -1, SQLITE_STATIC);

            if (stmt.Next()) {
                const char *sha256 = (const char *)sqlite3_column_text(stmt, 0);
                sha256 = sha256 ? sha256 : "";

                if (!TestStr(client_sha256, sha256)) {
                    LogError("Deletion refused because of sha256 mismatch");
                    io->AttachError(409);
                    return sq_TransactionResult::Rollback;
                }
            } else {
                if (stmt.IsValid() && client_sha256[0]) {
                    LogError("Deletion refused because of sha256 mismatch");
                    io->AttachError(409);
                }

                return sq_TransactionResult::Rollback;
            }
        }

        if (!instance->db.Run(R"(UPDATE fs_files SET blob = NULL, compression = NULL, sha256 = NULL
                                 WHERE path = ? AND sha256 IS NOT NULL)", request.url))
            return sq_TransactionResult::Error;

        if (sqlite3_changes(instance->db)) {
            io->AttachText(200, "Done!");
        } else {
            io->AttachError(404);
        }
        return sq_TransactionResult::Commit;
    });

    switch (ret) {
        case sq_TransactionResult::Commit: { io->AttachText(200, "Done!"); } break;
        case sq_TransactionResult::Rollback: { io->AttachError(404); } break;
        case sq_TransactionResult::Error: { /* Error 500 */ } break;
    }
}

}
