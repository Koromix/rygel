// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifdef _WIN32
    typedef unsigned int uid_t;
    typedef unsigned int gid_t;
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace RG {

static bool CheckKeyName(Span<const char> key, const char *type)
{
    const auto test_key_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_'; };

    if (!key.len) {
        LogError("%1 key cannot be empty", type);
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_key_char)) {
        LogError("%1 key must only contain lowercase alphanumeric or '_' characters", type);
        return false;
    }

    return true;
}

static bool CheckUserName(Span<const char> username)
{
    const auto test_user_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_' || c == '.'; };

    if (!username.len) {
        LogError("Username cannot be empty");
        return false;
    }
    if (!std::all_of(username.begin(), username.end(), test_user_char)) {
        LogError("Username must only contain alphanumeric, '_' or '.' characters");
        return false;
    }

    return true;
}

static bool HashPassword(Span<const char> password, char out_hash[crypto_pwhash_STRBYTES])
{
    if (crypto_pwhash_str(out_hash, password.ptr, password.len,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
        return false;
    }

    return true;
}

static bool ChangeFileOwner(const char *filename, uid_t uid, gid_t gid)
{
#ifndef _WIN32
    if (chown(filename, uid, gid) < 0) {
        LogError("Failed to change '%1' owner: %2", filename, strerror(errno));
        return false;
    }
#endif

    return true;
}

void HandleCreateInstance(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to create instances");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance_key = values.FindValue("key", nullptr);
        const char *app_key = values.FindValue("app_key", instance_key);
        const char *app_name = values.FindValue("app_name", instance_key);
        if (!instance_key) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (!CheckKeyName(instance_key, "Instance")) {
            io->AttachError(422);
            return;
        }

        // Check for existing instance
        {
            sq_Statement stmt;
            if (!goupile_domain.db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?;", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, instance_key, -1, SQLITE_STATIC);

            if (stmt.Next()) {
                LogError("Instance '%1' already exists", instance_key);
                io->AttachError(409);
                return;
            } else if (!stmt.IsValid()) {
                return;
            }
        }

        const char *database_filename = goupile_domain.config.GetInstanceFileName(instance_key, &io->allocator);
        if (TestFile(database_filename)) {
            LogError("Database '%1' already exists (old deleted instance?)", database_filename);
            io->AttachError(409);
            return;
        }
        RG_DEFER_N(db_guard) { UnlinkFile(database_filename); };

        uid_t owner_uid = 0;
        gid_t owner_gid = 0;
    #ifndef _WIN32
        {
            struct stat sb;
            if (stat(goupile_domain.config.database_filename, &sb) < 0) {
                LogError("Failed to stat '%1': %2", goupile_domain.config.database_filename, strerror(errno));
                return;
            }

            owner_uid = sb.st_uid;
            owner_gid = sb.st_gid;
        }
    #endif

        // Create instance database
        sq_Database db;
        if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return;
        if (!MigrateInstance(&db))
            return;
        if (!ChangeFileOwner(database_filename, owner_uid, owner_gid))
            return;

        // Set default settings
        {
            const char *sql = "UPDATE fs_settings SET value = ? WHERE key = ?";
            bool success = true;

            success &= db.Run(sql, app_key, "AppKey");
            success &= db.Run(sql, app_name, "AppName");

            if (!success)
                return;
        }

        // Create default files
        {
            Span<const AssetInfo> assets = GetPackedAssets();
            int64_t mtime = GetUnixTime();

            sq_Statement stmt;
            if (!db.Prepare(R"(INSERT INTO fs_files (active, path, mtime, blob, compression, sha256, size)
                               VALUES (1, ?, ?, ?, ?, ?, ?);)", &stmt))
                return;

            for (const AssetInfo &asset: assets) {
                if (StartsWith(asset.name, "demo/")) {
                    const char *path = Fmt(&io->allocator, "/files/%1", asset.name + 5).ptr;

                    HeapArray<uint8_t> gzip;
                    char sha256[65];
                    {
                        StreamReader reader(asset.data, "<asset>", asset.compression_type);
                        StreamWriter writer(&gzip, "<gzip>", CompressionType::Gzip);

                        crypto_hash_sha256_state state;
                        crypto_hash_sha256_init(&state);

                        while (!reader.IsEOF()) {
                            LocalArray<uint8_t, 16384> buf;
                            buf.len = reader.Read(buf.data);
                            if (buf.len < 0)
                                return;

                            writer.Write(buf);
                            crypto_hash_sha256_update(&state, buf.data, buf.len);
                        }

                        bool success = writer.Close();
                        RG_ASSERT(success);

                        uint8_t hash[crypto_hash_sha256_BYTES];
                        crypto_hash_sha256_final(&state, hash);
                        FormatSha256(hash, sha256);
                    }

                    stmt.Reset();
                    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 2, mtime);
                    sqlite3_bind_blob64(stmt, 3, gzip.ptr, gzip.len, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 4, "Gzip", -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 5, sha256, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 6, asset.data.len);

                    if (!stmt.Run())
                        return;
                }
            }
        }

        if (!db.Close())
            return;
        if (!goupile_domain.db.Run("INSERT INTO dom_instances (instance) VALUES (?);", instance_key))
            return;

        db_guard.Disable();
        io->AttachText(200, "Done!");
    });
}

void HandleDeleteInstance(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to delete instances");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance_key = values.FindValue("key", nullptr);
        if (!instance_key) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }

        sq_TransactionResult ret = goupile_domain.db.Transaction([&]() {
            if (!goupile_domain.db.Run("DELETE FROM dom_permissions WHERE instance = ?;", instance_key))
                return sq_TransactionResult::Error;
            if (!goupile_domain.db.Run("DELETE FROM dom_instances WHERE instance = ?;", instance_key))
                return sq_TransactionResult::Error;

            return sq_TransactionResult::Commit;
        });
        if (ret != sq_TransactionResult::Commit)
            return;
        if (!sqlite3_changes(goupile_domain.db)) {
            LogError("Instance '%1' does not exist", instance_key);
            io->AttachError(404);
            return;
        }

        io->AttachText(200, "Done!");
    });
}

void HandleListInstances(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list instances");
        io->AttachError(403);
        return;
    }

    std::shared_lock<std::shared_mutex> lock(goupile_domain.instances_mutex);

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (InstanceGuard *guard: goupile_domain.instances) {
        json.String(guard->instance.key);
    }
    json.EndArray();

    json.Finish(io);
}

void HandleCreateUser(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to create users");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        // Username and password
        const char *username = values.FindValue("username", nullptr);
        const char *password = values.FindValue("password", nullptr);
        if (!username || !password) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (!CheckUserName(username)) {
            io->AttachError(422);
            return;
        }
        if (!password[0]) {
            LogError("Empty password is not allowed");
            io->AttachError(422);
            return;
        }

        // Create admin user?
        bool admin = false;
        {
            const char *str = values.FindValue("admin", nullptr);
            if (str && !ParseBool(str, &admin)) {
                io->AttachError(422);
                return;
            }
        }

        // Check for existing user
        {
            sq_Statement stmt;
            if (!goupile_domain.db.Prepare("SELECT admin FROM dom_users WHERE username = ?", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

            if (stmt.Next()) {
                LogError("User '%1' already exists", username);
                io->AttachError(409);
                return;
            } else if (!stmt.IsValid()) {
                return;
            }
        }

        // Hash password
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(password, hash))
            return;

        // Create user
        if (!goupile_domain.db.Run("INSERT INTO dom_users (username, password_hash, admin) VALUES (?, ?, ?);",
                                   username, hash, 0 + admin))
            return;

        io->AttachText(200, "Done!");
    });
}

void HandleDeleteUser(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to delete users");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        // Username and password
        const char *username = values.FindValue("username", nullptr);
        if (!username) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }

        if (!goupile_domain.db.Run("DELETE FROM dom_users WHERE username = ?", username))
            return;
        if (!sqlite3_changes(goupile_domain.db)) {
            LogError("User '%1' does not exist", username);
            io->AttachError(404);
            return;
        }

        io->AttachError(200, "Done!");
    });
}

void HandleListUsers(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list users");
        io->AttachError(403);
        return;
    }

    sq_Statement stmt;
    if (!goupile_domain.db.Prepare(R"(SELECT u.rowid, u.username, u.admin, p.instance, p.permissions FROM dom_users u
                                      LEFT JOIN dom_permissions p ON (p.username = u.username)
                                      ORDER BY u.rowid, p.instance;)", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    if (stmt.Next()) {
        do {
            int64_t rowid = sqlite3_column_int64(stmt, 0);

            json.StartObject();

            json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 1));
            json.Key("admin"); json.Bool(sqlite3_column_int(stmt, 2));
            json.Key("instances"); json.StartArray();
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                do {
                    uint32_t permissions = (uint32_t)sqlite3_column_int64(stmt, 4);

                    json.StartObject();
                    json.Key("instance"); json.String((const char *)sqlite3_column_text(stmt, 3));
                    json.Key("permissions"); json.StartObject();
                    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                        char js_name[64];
                        ConvertToJsonName(UserPermissionNames[i], js_name);

                        json.Key(js_name); json.Bool(permissions & (1 << i));
                    }
                    json.EndObject();

                    json.EndObject();
                } while (stmt.Next() && sqlite3_column_int64(stmt, 0) == rowid);
            } else {
                stmt.Next();
            }
            json.EndArray();

            json.EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

}
