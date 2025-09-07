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
#include "admin.hh"
#include "domain.hh"
#include "file.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "src/core/password/password.hh"
#include "src/core/wrap/json.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"

#include <time.h>
#if defined(_WIN32)
    #include <io.h>

    typedef unsigned int uid_t;
    typedef unsigned int gid_t;
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <pwd.h>
#endif

namespace K {

static const char *DefaultConfig =
R"([Domain]
Title = %1
# DemoMode = Off

[Data]
# RootDirectory = .
# DatabaseFile = goupile.db
# ArchiveDirectory = archives
# SnapshotDirectory = snapshots
# SynchronousFull = Off
# UseSnapshots = On
# AutoCreate = On
# AutoMigrate = On

[Archives]
PublicKey = %2
# AutoHour = 0
# RetentionDays = 7

[Defaults]
# DefaultUser =
# DefaultPassword =

[Security]
# UserPassword = Moderate
# AdminPassword = Moderate
# RootPassword = Hard

[SMS]
# Provider = Twilio
# AuthID = <AuthID>
# AuthToken = <AuthToken>
# From = <Phone number or alphanumeric sender>

[SMTP]
# URL = <Curl URL>
# Username = <Username> (if any)
# Password = <Password> (if any)
# From = <Sender email address>

[HTTP]
# SocketType = Dual
# Port = 8889
# Threads =
# AsyncThreads =
# ClientAddress = Socket
)";

#pragma pack(push, 1)
struct ArchiveIntro {
    char signature[15];
    int8_t version;
    uint8_t eskey[crypto_secretstream_xchacha20poly1305_KEYBYTES + crypto_box_SEALBYTES];
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
};
#pragma pack(pop)
#define ARCHIVE_VERSION 1
#define ARCHIVE_SIGNATURE "GOUPILE_BACKUP"

enum class InstanceFlag {
    DefaultProject = 1 << 0,
    DemoInstance = 1 << 1
};

static bool CheckInstanceKey(Span<const char> key)
{
    const auto test_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '-'; };

    // Skip master prefix
    key = SplitStrReverse(key, '/');

    if (!key.len) {
        LogError("Instance key cannot be empty");
        return false;
    }
    if (key.len > 24) {
        LogError("Instance key cannot have more than 64 characters");
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_char)) {
        LogError("Instance key must only contain lowercase alphanumeric or '-' characters");
        return false;
    }

    static const char *const reserved_names[] = {
        "admin",
        "goupile",
        "metrics",
        "main",
        "static",
        "files",
        "blobs",
        "help"
    };

    // Reserved names
    if (std::find_if(std::begin(reserved_names), std::end(reserved_names),
                     [&](const char *name) { return key == name; }) != std::end(reserved_names)) {
        LogError("The following keys are not allowed: %1", FmtSpan(reserved_names));
        return false;
    }

    return true;
}

static bool CheckUserName(Span<const char> username)
{
    const auto test_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_' || c == '.' || c == '-'; };

    if (!username.len) {
        LogError("Username cannot be empty");
        return false;
    }
    if (username.len > 64) {
        LogError("Username cannot be have more than 64 characters");
        return false;
    }
    if (!std::all_of(username.begin(), username.end(), test_char)) {
        LogError("Username must only contain lowercase alphanumeric, '_', '.' or '-' characters");
        return false;
    }
    if (username == "goupile") {
        LogError("These usernames are forbidden: goupile");
        return false;
    }

    return true;
}

#if !defined(_WIN32)
static bool FindPosixUser(const char *username, uid_t *out_uid, gid_t *out_gid)
{
    struct passwd pwd_buf;
    HeapArray<char> buf;
    struct passwd *pwd;

again:
    buf.Grow(1024);
    buf.len += 1024;

    int ret = getpwnam_r(username, &pwd_buf, buf.ptr, buf.len, &pwd);
    if (ret != 0) {
        if (ret == ERANGE)
            goto again;

        LogError("getpwnam('%1') failed: %2", username, strerror(errno));
        return false;
    }
    if (!pwd) {
        LogError("Could not find system user '%1'", username);
        return false;
    }

    *out_uid = pwd->pw_uid;
    *out_gid = pwd->pw_gid;
    return true;
}
#endif

#if defined(_WIN32)

static bool ChangeFileOwner(const char *, uid_t, gid_t)
{
    return true;
}

#else

static bool ChangeFileOwner(const char *filename, uid_t uid, gid_t gid)
{
    if (chown(filename, uid, gid) < 0) {
        LogError("Failed to change '%1' owner: %2", filename, strerror(errno));
        return false;
    }

    return true;
}

#endif

static bool CreateInstance(DomainHolder *domain, const char *instance_key,
                           const char *name, int64_t userid, uint32_t permissions,
                           unsigned int flags, int *out_error)
{
    BlockAllocator temp_alloc;

    *out_error = 500;

    // Check for existing instance
    {
        sq_Statement stmt;
        if (!domain->db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?1", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, instance_key, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            LogError("Instance '%1' already exists", instance_key);
            *out_error = 409;
            return false;
        } else if (!stmt.IsValid()) {
            return false;
        }
    }

    const char *database_filename = MakeInstanceFileName(domain->config.instances_directory, instance_key, &temp_alloc);
    if (TestFile(database_filename)) {
        LogError("Database '%1' already exists (old deleted instance?)", database_filename);
        *out_error = 409;
        return false;
    }
    K_DEFER_N(db_guard) { UnlinkFile(database_filename); };

    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
#if !defined(_WIN32)
    {
        struct stat sb;
        if (stat(domain->config.database_filename, &sb) < 0) {
            LogError("Failed to stat '%1': %2", database_filename, strerror(errno));
            return false;
        }

        owner_uid = sb.st_uid;
        owner_gid = sb.st_gid;
    }
#endif

    // Create instance database
    sq_Database db;
    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!db.SetWAL(true))
        return false;
    if (!MigrateInstance(&db, InstanceVersion))
        return false;
    if (!ChangeFileOwner(database_filename, owner_uid, owner_gid))
        return false;

    // Set default settings
    {
        const char *sql = "UPDATE fs_settings SET value = ?2 WHERE key = ?1";
        bool success = true;

        success &= db.Run(sql, "Name", name);

        if (!success)
            return false;
    }

    // Use same modification time for all files
    int64_t mtime = GetUnixTime();

    if (!db.Run(R"(INSERT INTO fs_versions (version, mtime, userid, username, atomic)
                   VALUES (1, ?1, 0, 'goupile', 1))", mtime))
        return false;
    if (!db.Run(R"(INSERT INTO fs_versions (version, mtime, userid, username, atomic)
                   VALUES (0, ?1, 0, 'goupile', 0))", mtime))
        return false;
    if (!db.Run("UPDATE fs_settings SET value = 1 WHERE key = 'FsVersion'"))
        return false;

    // Create default files
    if (flags & (int)InstanceFlag::DefaultProject) {
        sq_Statement stmt1;
        sq_Statement stmt2;
        if (!db.Prepare(R"(INSERT INTO fs_objects (sha256, mtime, compression, size, blob)
                           VALUES (?1, ?2, ?3, ?4, ?5))", &stmt1))
            return false;
        if (!db.Prepare(R"(INSERT INTO fs_index (version, filename, sha256)
                           VALUES (1, ?1, ?2))", &stmt2))
            return false;

        for (const AssetInfo &asset: GetEmbedAssets()) {
            if (StartsWith(asset.name, "src/goupile/server/default/")) {
                const char *filename = asset.name + 27;

                CompressionType compression_type = CanCompressFile(filename) ? CompressionType::Gzip
                                                                                : CompressionType::None;

                HeapArray<uint8_t> blob;
                char sha256[65];
                Size total_len = 0;
                {
                    StreamReader reader(asset.data, "<asset>", 0, asset.compression_type);
                    StreamWriter writer(&blob, "<blob>", 0, compression_type);

                    crypto_hash_sha256_state state;
                    crypto_hash_sha256_init(&state);

                    while (!reader.IsEOF()) {
                        LocalArray<uint8_t, 16384> buf;
                        buf.len = reader.Read(buf.data);
                        if (buf.len < 0)
                            return false;
                        total_len += buf.len;

                        writer.Write(buf);
                        crypto_hash_sha256_update(&state, buf.data, buf.len);
                    }

                    bool success = writer.Close();
                    K_ASSERT(success);

                    uint8_t hash[crypto_hash_sha256_BYTES];
                    crypto_hash_sha256_final(&state, hash);
                    FormatSha256(hash, sha256);
                }

                stmt1.Reset();
                stmt2.Reset();
                sqlite3_bind_text(stmt1, 1, sha256, -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt1, 2, mtime);
                sqlite3_bind_text(stmt1, 3, CompressionTypeNames[(int)compression_type], -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt1, 4, total_len);
                sqlite3_bind_blob64(stmt1, 5, blob.ptr, blob.len, SQLITE_STATIC);
                sqlite3_bind_text(stmt2, 1, filename, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt2, 2, sha256, -1, SQLITE_STATIC);

                if (!stmt1.Run())
                    return false;
                if (!stmt2.Run())
                    return false;
            }
        }

        bool success = db.RunMany(R"(
            INSERT INTO fs_index (version, filename, sha256)
                SELECT 0, filename, sha256 FROM fs_index WHERE version = 1;
        )");
        if (!success)
            return false;
    }

    if (!db.Close())
        return false;

    bool success = domain->db.Transaction([&]() {
        int64_t now = GetUnixTime();
        sq_Binding demo = (flags & (int)InstanceFlag::DemoInstance) ? sq_Binding(now) : sq_Binding();

        if (!domain->db.Run(R"(INSERT INTO dom_instances (instance, demo)
                               VALUES (?1, ?2))", instance_key, demo)) {
            // Master does not exist
            if (sqlite3_errcode(domain->db) == SQLITE_CONSTRAINT) {
                Span<const char> master = SplitStr(instance_key, '/');

                LogError("Master instance '%1' does not exist", master);
                *out_error = 404;
            }

            return false;
        }

        if (userid > 0) {
            K_ASSERT(permissions);

            if (!domain->db.Run(R"(INSERT INTO dom_permissions (userid, instance, permissions)
                                   VALUES (?1, ?2, ?3))",
                                userid, instance_key, permissions))
                return false;
        }

        return true;
    });
    if (!success)
        return false;

    db_guard.Disable();
    return true;
}

static void GenerateKeyPair(Span<char> out_decrypt, Span<char> out_archive)
{
    static_assert(crypto_box_PUBLICKEYBYTES == 32);
    static_assert(crypto_box_SECRETKEYBYTES == 32);

    K_ASSERT(out_decrypt.len >= 45);
    K_ASSERT(out_archive.len >= 45);

    uint8_t sk[crypto_box_SECRETKEYBYTES];
    uint8_t pk[crypto_box_PUBLICKEYBYTES];
    crypto_box_keypair(pk, sk);

    sodium_bin2base64(out_decrypt.ptr, out_decrypt.len, sk, K_SIZE(sk), sodium_base64_VARIANT_ORIGINAL);
    sodium_bin2base64(out_archive.ptr, out_archive.len, pk, K_SIZE(pk), sodium_base64_VARIANT_ORIGINAL);
}

static void LogKeyPair(const char *decrypt_key, const char *archive_key)
{
    LogInfo("Archive decryption key: %!..+%1%!0", decrypt_key);
    LogInfo("            Public key: %!..+%1%!0", archive_key);
    LogInfo();
    LogInfo("You need this key to restore Goupile archives, %!..+you must not lose it!%!0");
    LogInfo("There is no way to get it back, without it the archives are lost.");
}

static bool ParseKeyString(Span<const char> str, uint8_t out_key[32] = nullptr)
{
    static_assert(crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES == 32);

    uint8_t key[32];
    size_t key_len;
    int ret = sodium_base642bin(key, K_SIZE(key), str.ptr, (size_t)str.len,
                                nullptr, &key_len, nullptr, sodium_base64_VARIANT_ORIGINAL);
    if (ret || key_len != 32) {
        LogError("Malformed key value");
        return false;
    }

    if (out_key) {
        MemCpy(out_key, key, K_SIZE(key));
    }
    return true;
}

int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *title = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    bool check_password = true;
    bool totp = false;
    char decrypt_key[45] = {};
    char archive_key[45] = {};
    const char *demo = nullptr;
    bool change_owner = false;
    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
    const char *root_directory = nullptr;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 init [option...] [directory]%!0

Options:

    %!..+-t, --title title%!0              Set domain title

    %!..+-u, --username username%!0        Name of default user
        %!..+--password password%!0        Password of default user
        %!..+--totp%!0                     Enable TOTP for root user

        %!..+--allow_unsafe_password%!0    Allow unsafe passwords

        %!..+--archive_key key%!0          Set domain public archive key

        %!..+--demo [name]%!0              Create default instance)"), FelixTarget);

#if !defined(_WIN32)
        PrintLn(st, T(R"(
    %!..+-o, --owner owner%!0              Change directory and file owner)"));
#endif
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-t", "--title", OptionType::Value)) {
                title = opt.current_value;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--allow_unsafe_password")) {
                check_password = false;
            } else if (opt.Test("--totp")) {
                totp = true;
            } else if (opt.Test("--archive_key", OptionType::Value)) {
                if (!ParseKeyString(opt.current_value))
                    return 1;

                K_ASSERT(strlen(opt.current_value) < K_SIZE(archive_key));
                CopyString(opt.current_value, archive_key);
            } else if (opt.Test("--demo", OptionType::OptionalValue)) {
                demo = opt.current_value ? opt.current_value : "demo";
#if !defined(_WIN32)
            } else if (opt.Test("-o", "--owner", OptionType::Value)) {
                change_owner = true;

                if (!FindPosixUser(opt.current_value, &owner_uid, &owner_gid))
                    return 1;
#endif
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        root_directory = opt.ConsumeNonOption();
        root_directory = NormalizePath(root_directory ? root_directory : ".", GetWorkingDirectory(), &temp_alloc).ptr;

        opt.LogUnusedArguments();
    }

    // Errors and defaults
    if (password && !username) {
        LogError("Option --password cannot be used without --username");
        return 1;
    }

    DomainHolder domain;

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    K_DEFER_N(root_guard) {
        domain.db.Close();

        for (const char *filename: files) {
            UnlinkFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            UnlinkDirectory(directories[i]);
        }
    };

    // Make or check instance directory
    if (TestFile(root_directory)) {
        if (!IsDirectoryEmpty(root_directory)) {
            LogError("Directory '%1' exists and is not empty", root_directory);
            return 1;
        }
    } else {
        if (!MakeDirectory(root_directory))
            return 1;
        directories.Append(root_directory);
    }
    if (change_owner && !ChangeFileOwner(root_directory, owner_uid, owner_gid))
        return 1;

    // Gather missing information
    if (!title) {
        title = SplitStrReverseAny(root_directory, K_PATH_SEPARATORS).ptr;

retry_title:
        title = Prompt(T("Domain title:"), title, nullptr, &temp_alloc);
        if (!title)
            return 1;

        if (!CheckDomainTitle(title))
            goto retry_title;
    }
    if (!title[0]) {
        LogError("Empty domain title is not allowed");
        return 1;
    }
    if (!username) {
        username = Prompt(T("Admin user:"), &temp_alloc);
        if (!username)
            return 1;
    }
    if (!CheckUserName(username))
        return 1;
    if (password) {
        if (check_password && !pwd_CheckPassword(password, username))
            return 1;
    } else {
retry_pwd:
        password = Prompt(T("Admin password:"), nullptr, "*", &temp_alloc);
        if (!password)
            return 1;

        if (check_password && !pwd_CheckPassword(password, username))
            goto retry_pwd;

        const char *password2 = Prompt(T("Confirm:"), nullptr, "*", &temp_alloc);
        if (!password2)
            return 1;

        if (!TestStr(password, password2)) {
            LogError("Password mismatch");
            goto retry_pwd;
        }
    }
    LogInfo();

    // Create archive key pair
    if (!archive_key[0]) {
        GenerateKeyPair(archive_key, decrypt_key);
    }

    // Create domain config
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupile.ini", root_directory).ptr;
        files.Append(filename);

        StreamWriter writer(filename);
        Print(&writer, DefaultConfig, title, archive_key);
        if (!writer.Close())
            return 1;

        if (!LoadConfig(filename, &domain.config))
            return 1;
    }

    // Create directories
    {
        const auto make_directory = [&](const char *path) {
            if (!MakeDirectory(path))
                return false;
            directories.Append(path);
            if (change_owner && !ChangeFileOwner(path, owner_uid, owner_gid))
                return false;

            return true;
        };

        if (!make_directory(domain.config.instances_directory))
            return 1;
        if (!make_directory(domain.config.tmp_directory))
            return 1;
        if (!make_directory(domain.config.archive_directory))
            return 1;
        if (!make_directory(domain.config.snapshot_directory))
            return 1;
        if (!make_directory(domain.config.view_directory))
            return 1;
        if (!make_directory(domain.config.export_directory))
            return 1;
    }

    // Create database
    if (!domain.db.Open(domain.config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    files.Append(domain.config.database_filename);
    if (!domain.db.SetWAL(true))
        return 1;
    if (!MigrateDomain(&domain.db, domain.config.instances_directory))
        return 1;
    if (change_owner && !ChangeFileOwner(domain.config.database_filename, owner_uid, owner_gid))
        return 1;

    // Create default root user
    {
        char hash[PasswordHashBytes];
        if (!HashPassword(password, hash))
            return 1;

        // Create local key
        char local_key[45];
        {
            uint8_t buf[32];
            FillRandomSafe(buf);
            sodium_bin2base64(local_key, K_SIZE(local_key), buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        if (!domain.db.Run(R"(INSERT INTO dom_users (userid, username, password_hash,
                                                     change_password, root, local_key, confirm)
                              VALUES (1, ?1, ?2, 0, 1, ?3, ?4))", username, hash, local_key, totp ? "TOTP" : nullptr))
            return 1;
    }

    // Create default instance
    {
        unsigned int flags = (int)InstanceFlag::DefaultProject;
        uint32_t permissions = (1u << K_LEN(UserPermissionNames)) - 1;

        int dummy;
        if (demo && !CreateInstance(&domain, demo, demo, 1, permissions, flags, &dummy))
            return 1;
    }

    if (!domain.db.Close())
        return 1;

    if (decrypt_key[0]) {
        LogInfo();
        LogKeyPair(decrypt_key, archive_key);
    }

    root_guard.Disable();
    return 0;
}

int RunMigrate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "goupile.ini";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 migrate [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0)"), FelixTarget, config_filename);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/goupile.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    DomainConfig config;
    if (!LoadConfig(config_filename, &config))
        return 1;

    // Migrate and open main database
    sq_Database db;
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return 1;
    if (!MigrateDomain(&db, config.instances_directory))
        return 1;

    // Migrate instances
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT instance FROM dom_instances", &stmt))
            return 1;

        bool success = true;

        while (stmt.Step()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *filename = MakeInstanceFileName(config.instances_directory, key, &temp_alloc);

            success &= MigrateInstance(filename);
        }
        if (!stmt.IsValid())
            return 1;

        if (!success)
            return 1;
    }

    if (!db.Close())
        return 1;

    return 0;
}

int RunKeys(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    char decrypt_key[45] = {};
    char archive_key[45] = {};
    bool random_key = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 keys [option...]%!0

Options:

    %!..+-k, --decrypt_key [key]%!0        Use existing decryption key)"), FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-k", "--decrypt_key", OptionType::OptionalValue)) {
                if (opt.current_value) {
                    if (!ParseKeyString(opt.current_value))
                        return 1;

                    K_ASSERT(strlen(opt.current_value) < K_SIZE(decrypt_key));
                    CopyString(opt.current_value, decrypt_key);
                } else {
                    MemSet(decrypt_key, 0, K_SIZE(decrypt_key));
                }

                random_key = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (random_key) {
        GenerateKeyPair(decrypt_key, archive_key);
    } else {
        uint8_t sk[crypto_box_SECRETKEYBYTES];
        uint8_t pk[crypto_box_PUBLICKEYBYTES];

        if (!decrypt_key[0]) {
again:
            const char *key = Prompt(T("Decryption key:"), nullptr, "*", &temp_alloc);
            if (!key)
                return 1;
            if (!ParseKeyString(key, sk))
                goto again;
        } else {
            // Already checked it is well formed
            size_t key_len;
            int ret = sodium_base642bin(sk, K_SIZE(sk), decrypt_key, strlen(decrypt_key),
                                        nullptr, &key_len, nullptr, sodium_base64_VARIANT_ORIGINAL);
            K_ASSERT(!ret && key_len == 32);
        }

        crypto_scalarmult_base(pk, sk);

        sodium_bin2base64(decrypt_key, K_SIZE(decrypt_key), sk, K_SIZE(sk), sodium_base64_VARIANT_ORIGINAL);
        sodium_bin2base64(archive_key, K_SIZE(archive_key), pk, K_SIZE(pk), sodium_base64_VARIANT_ORIGINAL);
    }

    LogKeyPair(decrypt_key, archive_key);

    return 0;
}

enum class UnsealResult {
    Success,
    WrongKey,
    Error
};

static UnsealResult UnsealArchive(StreamReader *reader, StreamWriter *writer, const char *decrypt_key)
{
    // Derive asymmetric keys
    uint8_t askey[crypto_box_SECRETKEYBYTES];
    uint8_t apkey[crypto_box_PUBLICKEYBYTES];
    {
        static_assert(crypto_scalarmult_SCALARBYTES == crypto_box_SECRETKEYBYTES);
        static_assert(crypto_scalarmult_BYTES == crypto_box_PUBLICKEYBYTES);

        size_t key_len;
        int ret = sodium_base642bin(askey, K_SIZE(askey),
                                    decrypt_key, strlen(decrypt_key), nullptr, &key_len,
                                    nullptr, sodium_base64_VARIANT_ORIGINAL);
        if (ret || key_len != 32) {
            LogError("Malformed decryption key");
            return UnsealResult::Error;
        }

        crypto_scalarmult_base(apkey, askey);
    }

    // Read archive header
    ArchiveIntro intro;
    if (reader->Read(K_SIZE(intro), &intro) != K_SIZE(intro)) {
        if (reader->IsValid()) {
            LogError("Truncated archive");
        }
        return UnsealResult::Error;
    }

    // Check signature
    if (strncmp(intro.signature, ARCHIVE_SIGNATURE, K_SIZE(intro.signature)) != 0) {
        LogError("Unexpected archive signature");
        return UnsealResult::Error;
    }
    if (intro.version != ARCHIVE_VERSION) {
        LogError("Unexpected archive version %1 (expected %2)", intro.version, ARCHIVE_VERSION);
        return UnsealResult::Error;
    }

    // Decrypt symmetric key
    uint8_t skey[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    if (crypto_box_seal_open(skey, intro.eskey, K_SIZE(intro.eskey), apkey, askey) != 0) {
        LogError("Failed to unseal archive (wrong key?)");
        return UnsealResult::WrongKey;
    }

    // Init symmetric decryption
    crypto_secretstream_xchacha20poly1305_state state;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&state, intro.header, skey) != 0) {
        LogError("Failed to initialize symmetric decryption (corrupt archive?)");
        return UnsealResult::Error;
    }

    // Write cleartext ZIP archive
    for (;;) {
        LocalArray<uint8_t, 4096> cypher;
        cypher.len = reader->Read(cypher.data);
        if (cypher.len < 0)
            return UnsealResult::Error;

        uint8_t buf[4096];
        unsigned long long buf_len = 0;
        uint8_t tag;
        if (crypto_secretstream_xchacha20poly1305_pull(&state, buf, &buf_len, &tag,
                                                       cypher.data, cypher.len, nullptr, 0) != 0) {
            LogError("Failed during symmetric decryption (corrupt archive?)");
            return UnsealResult::Error;
        }

        if (!writer->Write(buf, (Size)buf_len))
            return UnsealResult::Error;

        if (reader->IsEOF()) {
            if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
                LogError("Truncated archive");
                return UnsealResult::Error;
            }
            break;
        }
    }

    return UnsealResult::Success;
}

int RunUnseal(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *archive_filename = nullptr;
    const char *output_filename = nullptr;
    const char *decrypt_key = nullptr;
    bool extract = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 unseal [option...] archive_file%!0

Options:

    %!..+-O, --output_file filename%!0     Set output file
    %!..+-k, --decrypt_key key%!0          Set decryption key

        %!..+--check%!0                    Only check that key is valid)"), FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                output_filename = opt.current_value;
            } else if (opt.Test("-k", "--key", OptionType::Value)) {
                decrypt_key = opt.current_value;
            } else if (opt.Test("--check")) {
                extract = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        archive_filename = opt.ConsumeNonOption();
        if (!archive_filename) {
            LogError("No archive filename provided");
            return 1;
        }
        opt.LogUnusedArguments();
    }

    StreamReader reader(archive_filename);
    if (!reader.IsValid())
        return 1;

    if (!output_filename) {
        Span<const char> extension = GetPathExtension(archive_filename);
        Span<const char> name = MakeSpan(archive_filename, extension.ptr - archive_filename);

        output_filename = Fmt(&temp_alloc, "%1.zip", name).ptr;
    }
    if (extract && TestFile(output_filename)) {
        LogError("File '%1' already exists", output_filename);
        return 1;
    }

    if (!decrypt_key) {
        decrypt_key = Prompt(T("Decryption key:"), nullptr, "*", &temp_alloc);
        if (!decrypt_key)
            return 1;
    }

    StreamWriter writer;
    if (extract) {
        writer.Open(output_filename, (int)StreamWriterFlag::Atomic | (int)StreamWriterFlag::Exclusive);
    } else {
        writer.Open([](Span<const uint8_t>) { return true; }, "<null>");
    }
    if (!writer.IsValid())
        return 1;

    if (UnsealArchive(&reader, &writer, decrypt_key) != UnsealResult::Success)
        return 1;
    if (!writer.Close())
        return 1;

    if (extract) {
        LogInfo("Unsealed archive: %!..+%1%!0", output_filename);
    } else {
        LogInfo("Key appears correct");
    }

    return 0;
}

void HandleDemoCreate(http_IO *io)
{
    if (!gp_domain.config.demo_mode) {
        LogError("Demo mode is not enabled");
        io->SendError(403);
        return;
    }

    char name[9];
    Fmt(name, "%1", FmtRandom(K_SIZE(name) - 1));

    bool success = gp_domain.db.Transaction([&]() {
        unsigned int flags = (int)InstanceFlag::DefaultProject | (int)InstanceFlag::DemoInstance;

        int error;
        if (!CreateInstance(&gp_domain, name, name, -1, 0, flags, &error)) {
            io->SendError(error);
            return false;
        }

        return true;
    });
    if (!success)
        return;

    if (!gp_domain.SyncInstance(name))
        return;

    InstanceHolder *instance = gp_domain.Ref(name);
    if (!instance)
        return;
    K_DEFER { instance->Unref(); };

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
    if (!session) [[unlikely]]
        return;
    SessionStamp *stamp = session->GetStamp(instance);
    if (!stamp) [[unlikely]] {
        LogError("Failed to set session mode");
        return;
    }
    stamp->develop = true;

    const char *redirect = Fmt(io->Allocator(), "/%1/", name).ptr;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("url"); json->String(redirect);

        json->EndObject();
    });
}

void PruneDemos()
{
    K_ASSERT(gp_domain.config.demo_mode);

    // Best effort
    gp_domain.db.Transaction([&] {
        int64_t treshold = GetUnixTime() - DemoPruneDelay;

        if (!gp_domain.db.Run("DELETE FROM dom_instances WHERE demo IS NOT NULL AND demo < ?1", treshold))
            return false;
        if (sqlite3_changes(gp_domain.db) && !gp_domain.SyncAll(true))
            return false;

        return true;
    });
}

void HandleInstanceCreate(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to create instances");
        io->SendError(403);
        return;
    }

    if (gp_domain.CountInstances() >= MaxInstancesPerDomain) {
        LogError("This domain has too many instances");
        io->SendError(403);
        return;
    }

    const char *instance_key = nullptr;
    const char *name = nullptr;
    bool populate = false;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "key") {
                parser.ParseString(&instance_key);
            } else if (key == "name") {
                parser.SkipNull() || parser.ParseString(&name);
            } else if (key == "populate") {
                parser.ParseBool(&populate);
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

        if (!instance_key) {
            LogError("Missing 'key' parameter");
            valid = false;
        } else if (!CheckInstanceKey(instance_key)) {
            valid = false;
        }
        if (!name) {
            name = instance_key;
        } else if (!name[0]) {
            LogError("Application name cannot be empty");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Can this admin user touch this instance?
    if (!session->IsRoot()) {
        if (!strchr(instance_key, '/')) {
            LogError("Instance '%1' does not exist", instance_key);
            io->SendError(404);
            return;
        }

        Span<const char> master = SplitStr(instance_key, '/');

        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND instance = ?2 AND
                                           permissions & ?3)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_text(stmt, 2, master.ptr, (int)master.len, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance_key);
                io->SendError(404);
            }
            return;
        }
    }

    bool success = gp_domain.db.Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "create_instance", session->username,
                              instance_key))
            return false;

        unsigned int flags = populate ? (int)InstanceFlag::DefaultProject : 0;
        uint32_t permissions = (1u << K_LEN(UserPermissionNames)) - 1;

        int error;
        if (!CreateInstance(&gp_domain, instance_key, name, session->userid, permissions, flags, &error)) {
            io->SendError(error);
            return false;
        }

        return true;
    });
    if (!success)
        return;

    if (!gp_domain.SyncInstance(instance_key))
        return;

    io->SendText(200, "{}", "application/json");
}

static bool ArchiveInstances(const InstanceHolder *filter, bool *out_conflict = nullptr)
{
    BlockAllocator temp_alloc;

    Span<InstanceHolder *> instances = gp_domain.LockInstances();
    K_DEFER { gp_domain.UnlockInstances(); };

    if (out_conflict) {
        *out_conflict = false;
    }

    struct BackupEntry {
        sq_Database *db;
        const char *basename;
        const char *filename;
    };

    HeapArray<BackupEntry> entries;
    K_DEFER { for (const BackupEntry &entry: entries) UnlinkFile(entry.filename); };

    // Make archive filename
    const char *archive_filename;
    {
        time_t mtime = (time_t)(GetUnixTime() / 1000);

#if defined(_WIN32)
        struct tm mtime_tm;
        {
            errno_t err = _gmtime64_s(&mtime_tm, &mtime);
            if (err) {
                LogError("Failed to format current time: %1", strerror(err));
                return false;
            }
        }
#else
        struct tm mtime_tm;
        if (!gmtime_r(&mtime, &mtime_tm)) {
            LogError("Failed to format current time: %1", strerror(errno));
            return false;
        }
#endif

        char mtime_str[128];
        if (!strftime(mtime_str, K_SIZE(mtime_str), "%Y%m%dT%H%M%S%z", &mtime_tm)) {
            LogError("Failed to format current time: strftime failed");
            return false;
        }

        HeapArray<char> buf(&temp_alloc);
        Fmt(&buf, "%1%/%2_%3", gp_domain.config.archive_directory, gp_domain.config.title, mtime_str);
        if (filter) {
            const char *filename = sqlite3_db_filename(*filter->db, "main");

            Span<const char> basename = SplitStrReverseAny(filename, K_PATH_SEPARATORS);
            SplitStrReverse(basename, '.', &basename);

            Fmt(&buf, "+%1.goarch", basename);
        } else {
            Fmt(&buf, ".goarch");
        }

        archive_filename = buf.Leak().ptr;
    }

    // Open archive
    StreamWriter writer;
    if (!writer.Open(archive_filename, (int)StreamWriterFlag::Exclusive |
                                       (int)StreamWriterFlag::Atomic)) {
        if (out_conflict && errno == EEXIST) {
            *out_conflict = true;
        }
        return false;
    }

    // Generate backup entries
    entries.Append({ &gp_domain.db, "goupile.db", nullptr });
    for (InstanceHolder *instance: instances) {
        if (filter == nullptr || instance == filter || instance->master == filter) {
            const char *filename = sqlite3_db_filename(*instance->db, "main");

            const char *basename = SplitStrReverseAny(filename, K_PATH_SEPARATORS).ptr;
            basename = Fmt(&temp_alloc, "instances/%1", basename).ptr;

            entries.Append({ instance->db, basename, nullptr });
        }
    }
    for (BackupEntry &entry: entries) {
        entry.filename = CreateUniqueFile(gp_domain.config.tmp_directory, nullptr, ".tmp", &temp_alloc);
        if (!entry.filename)
            return false;
    }

    // Backup databases
    for (const BackupEntry &entry: entries) {
        if (!entry.db->BackupTo(entry.filename))
            return false;
    }

    // Closure context for miniz write callback
    struct BackupContext {
        StreamWriter *writer;
        crypto_secretstream_xchacha20poly1305_state state;
        LocalArray<uint8_t, 4096 - crypto_secretstream_xchacha20poly1305_ABYTES> buf;
    };
    BackupContext ctx = {};
    ctx.writer = &writer;

    // Write archive intro
    {
        ArchiveIntro intro = {};

        CopyString(ARCHIVE_SIGNATURE, intro.signature);
        intro.version = ARCHIVE_VERSION;

        uint8_t skey[crypto_secretstream_xchacha20poly1305_KEYBYTES];
        crypto_secretstream_xchacha20poly1305_keygen(skey);
        if (crypto_secretstream_xchacha20poly1305_init_push(&ctx.state, intro.header, skey) != 0) {
            LogError("Failed to initialize symmetric encryption");
            return false;
        }
        if (crypto_box_seal(intro.eskey, skey, K_SIZE(skey), gp_domain.config.archive_key) != 0) {
            LogError("Failed to seal symmetric key");
            return false;
        }

        if (!writer.Write(&intro, K_SIZE(intro)))
            return false;
    }

    // Init ZIP compressor
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    zip.m_pWrite = [](void *udata, mz_uint64, const void *buf, size_t len) {
        BackupContext *ctx = (BackupContext *)udata;
        size_t copy = len;

        while (len) {
            Size copy_len = std::min((Size)len, ctx->buf.Available());
            MemCpy(ctx->buf.end(), buf, copy_len);
            ctx->buf.len += copy_len;

            if (!ctx->buf.Available()) {
                uint8_t cypher[4096];
                unsigned long long cypher_len;
                if (crypto_secretstream_xchacha20poly1305_push(&ctx->state, cypher, &cypher_len,
                                                               ctx->buf.data, ctx->buf.len, nullptr, 0, 0) != 0) {
                    LogError("Failed during symmetric encryption");
                    return (size_t)-1;
                }
                if (!ctx->writer->Write(cypher, (Size)cypher_len))
                    return (size_t)-1;
                ctx->buf.len = 0;
            }

            buf = (const void *)((const uint8_t *)buf + copy_len);
            len -= copy_len;
        }

        return copy;
    };
    zip.m_pIO_opaque = &ctx;
    if (!mz_zip_writer_init(&zip, 0)) {
        LogError("Failed to create ZIP archive: %1", mz_zip_get_error_string(zip.m_last_error));
        return false;
    }
    K_DEFER { mz_zip_writer_end(&zip); };

    // Add databases to ZIP archive
    for (const BackupEntry &entry: entries) {
        if (!mz_zip_writer_add_file(&zip, entry.basename, entry.filename, nullptr, 0, MZ_BEST_SPEED)) {
            if (zip.m_last_error != MZ_ZIP_WRITE_CALLBACK_FAILED) {
                LogError("Failed to compress '%1': %2", entry.basename, mz_zip_get_error_string(zip.m_last_error));
            }
            return false;
        }
    }

    // Finalize ZIP
    if (!mz_zip_writer_finalize_archive(&zip)) {
        if (zip.m_last_error != MZ_ZIP_WRITE_CALLBACK_FAILED) {
            LogError("Failed to finalize ZIP archive: %1", mz_zip_get_error_string(zip.m_last_error));
        }
        return false;
    }

    // Finalize encryption
    {
        uint8_t cypher[4096];
        unsigned long long cypher_len;
        if (crypto_secretstream_xchacha20poly1305_push(&ctx.state, cypher, &cypher_len, ctx.buf.data, ctx.buf.len, nullptr, 0,
                                                       crypto_secretstream_xchacha20poly1305_TAG_FINAL) != 0) {
            LogError("Failed during symmetric encryption");
            return false;
        }

        if (!writer.Write(cypher, (Size)cypher_len))
            return false;
     }

    // Flush buffers and rename atomically
    if (!writer.Close())
        return false;

    return true;
}

bool ArchiveDomain()
{
    bool conflict = false;
    return ArchiveInstances(nullptr, &conflict) || conflict;
}

void HandleInstanceDelete(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to delete instances");
        io->SendError(403);
        return;
    }

    const char *instance_key = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "instance") {
                parser.ParseString(&instance_key);
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

    // Check missing values
    if (!instance_key) {
        LogError("Missing 'instance' parameter");
        io->SendError(422);
        return;
    }

    InstanceHolder *instance = gp_domain.Ref(instance_key);
    if (!instance) {
        LogError("Instance '%1' does not exist", instance_key);
        io->SendError(404);
        return;
    }
    K_DEFER_N(ref_guard) { instance->Unref(); };

    // Can this admin user touch this instance?
    if (!session->IsRoot()) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND instance = ?2 AND
                                           permissions & ?3)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_text(stmt, 2, instance->master->key.ptr, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance_key);
                io->SendError(404);
            }
            return;
        }
    }

    // Be safe...
    {
        bool conflict;
        if (!ArchiveInstances(instance, &conflict)) {
            if (conflict) {
                io->SendError(409, "Archive already exists");
            }
            return;
        }
    }

    // Copy filenames to avoid use-after-free
    HeapArray<const char *> unlink_filenames;
    {
        for (const InstanceHolder *slave: instance->slaves) {
            const char *filename = DuplicateString(sqlite3_db_filename(*slave->db, "main"), io->Allocator()).ptr;
            unlink_filenames.Append(filename);
        }

        const char *filename = DuplicateString(sqlite3_db_filename(*instance->db, "main"), io->Allocator()).ptr;
        unlink_filenames.Append(filename);
    }

    bool success = gp_domain.db.Transaction([&]() {
        int64_t time = GetUnixTime();

        for (Size i = instance->slaves.len - 1; i >= 0; i--) {
            InstanceHolder *slave = instance->slaves[i];

            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5))",
                                  time, request.client_addr, "delete_instance", session->username,
                                  slave->key))
                return false;
            if (!gp_domain.db.Run("DELETE FROM dom_instances WHERE instance = ?1", slave->key))
                return false;
        }

        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "delete_instance", session->username,
                              instance_key))
            return false;
        if (!gp_domain.db.Run("DELETE FROM dom_instances WHERE instance = ?1", instance_key))
            return false;

        // Don't use instance after that!
        instance->Unref();
        instance = nullptr;
        ref_guard.Disable();

        return true;
    });
    if (!success)
        return;

    if (!gp_domain.SyncInstance(instance_key))
        return;

    bool complete = true;
    for (const char *filename: unlink_filenames) {
        // Not much we can do if this fails to succeed anyway; the backup is okay and the
        // instance is deleted. We're mostly successful and we can't go back.

        // Switch off WAL to make sure new databases with the same name don't get into trouble
        sq_Database db;
        if (!db.Open(filename, SQLITE_OPEN_READWRITE)) {
            complete = false;
            continue;
        }
        if (!db.RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                           PRAGMA journal_mode = DELETE;)")) {
            complete = false;
            continue;
        }
        db.Close();

        complete &= UnlinkFile(filename);
    }

    io->SendText(complete ? 200 : 202, "{}", "application/json");
}

void HandleInstanceConfigure(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to configure instances");
        io->SendError(403);
        return;
    }

    const char *instance_key = nullptr;
    decltype(InstanceHolder::config) config;
    bool change_use_offline = false;
    bool change_data_remote = false;
    bool change_allow_guests = false;
    bool change_export = false;
    int64_t fs_version = -1;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "instance") {
                parser.ParseString(&instance_key);
            } else if (key == "name") {
                parser.SkipNull() || parser.ParseString(&config.name);
            } else if (key == "use_offline") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&config.use_offline);
                    change_use_offline = true;
                }
            } else if (key == "data_remote") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&config.data_remote);
                    change_data_remote = true;
                }
            } else if (key == "token_key") {
                parser.SkipNull() || parser.ParseString(&config.token_key);
            } else if (key == "auto_key") {
                parser.SkipNull() || parser.ParseString(&config.auto_key);
            } else if (key == "allow_guests") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&config.allow_guests);
                    change_allow_guests = true;
                }
            } else if (key == "fs_version") {
                parser.SkipNull() || parser.ParseInt(&fs_version);
            } else if (key == "export_days") {
                if (!parser.SkipNull()) {
                    parser.ParseInt(&config.export_days);
                    change_export = true;
                }
            } else if (key == "export_time") {
                if (!parser.SkipNull()) {
                    parser.ParseInt(&config.export_time);
                    change_export = true;
                }
            } else if (key == "export_all") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&config.export_all);
                    change_export = true;
                }
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

        if (!instance_key) {
            LogError("Missing 'instance' parameter");
            valid = false;
        }
        if (config.name && !config.name[0]) {
            LogError("Application name cannot be empty");
            valid = false;
        }
        if (change_export) {
            if (config.export_days < 0 || config.export_days > 127) {
                LogError("Invalid value for export days");
                valid = false;
            }
            if (config.export_time < 0 || config.export_time >= 2400) {
                LogError("Invalid value for export time");
                valid = false;
            }
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    InstanceHolder *instance = gp_domain.Ref(instance_key);
    if (!instance) {
        LogError("Instance '%1' does not exist", instance_key);
        io->SendError(404);
        return;
    }
    K_DEFER_N(ref_guard) { instance->Unref(); };

    // Can this admin user touch this instance?
    if (!session->IsRoot()) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND instance = ?2 AND
                                           permissions & ?3)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_text(stmt, 2, instance->master->key.ptr, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance_key);
                io->SendError(404);
            }
            return;
        }
    }

    // Write new configuration to database
    bool success = instance->db->Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "edit_instance", session->username,
                              instance_key))
            return false;

        const char *sql = "UPDATE fs_settings SET value = ?2 WHERE key = ?1";
        bool success = true;

        success &= !config.name || instance->db->Run(sql, "Name", config.name);
        if (instance->master == instance) {
            success &= !change_use_offline || instance->db->Run(sql, "UseOffline", 0 + config.use_offline);
            success &= !change_data_remote || instance->db->Run(sql, "DataRemote", 0 + config.data_remote);
            success &= !config.token_key || instance->db->Run(sql, "TokenKey", config.token_key);
            success &= !config.auto_key || instance->db->Run(sql, "AutoKey", config.auto_key);
            success &= !change_allow_guests || instance->db->Run(sql, "AllowGuests", 0 + config.allow_guests);
            success &= !change_export || instance->db->Run(sql, "ExportDays", config.export_days);
            success &= !change_export || instance->db->Run(sql, "ExportTime", config.export_time);
            success &= !change_export || instance->db->Run(sql, "ExportAll", 0 + config.export_all);

            if (fs_version > 0) {
                success &= instance->db->Run(sql, "FsVersion", fs_version);

                // Copy to test version
                if (!instance->db->Run(R"(UPDATE fs_versions SET mtime = copy.mtime,
                                                                 userid = copy.userid,
                                                                 username = copy.username
                                              FROM (SELECT mtime, userid, username FROM fs_versions WHERE version = ?1) AS copy
                                              WHERE version = 0)",
                                       fs_version))
                    return false;
                if (!instance->db->Run(R"(DELETE FROM fs_index WHERE version = 0)"))
                    return false;
                if (!instance->db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                              SELECT 0, filename, sha256 FROM fs_index WHERE version = ?1)", fs_version))
                    return false;
            }
        }
        if (!success)
            return false;

        // Don't use instance after that!
        instance->Unref();
        instance = nullptr;
        ref_guard.Disable();

        return true;
    });
    if (!success)
        return;

    if (!gp_domain.SyncInstance(instance_key))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleInstanceList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to list instances");
        io->SendError(403);
        return;
    }

    Span<InstanceHolder *> instances = gp_domain.LockInstances();
    K_DEFER { gp_domain.UnlockInstances(); };

    // Check allowed instances
    HashSet<const char *> allowed_masters;
    if (!session->IsRoot()) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND permissions & ?2)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_int(stmt, 2, (int)UserPermission::BuildAdmin);

        while (stmt.Step()) {
            const char *instance_key = (const char *)sqlite3_column_text(stmt, 0);
            allowed_masters.Set(DuplicateString(instance_key, io->Allocator()).ptr);
        }
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        for (InstanceHolder *instance: instances) {
            if (!session->IsRoot() && !allowed_masters.Find(instance->master->key.ptr))
                continue;

            json->StartObject();

            json->Key("key"); json->String(instance->key.ptr);
            if (instance->master != instance) {
                json->Key("master"); json->String(instance->master->key.ptr);
            } else {
                json->Key("slaves"); json->Int64(instance->slaves.len);
            }
            json->Key("legacy"); json->Bool(instance->legacy);
            json->Key("config"); json->StartObject();
                json->Key("name"); json->String(instance->config.name);
                json->Key("use_offline"); json->Bool(instance->config.use_offline);
                json->Key("data_remote"); json->Bool(instance->config.data_remote);
                if (instance->config.token_key) {
                    json->Key("token_key"); json->String(instance->config.token_key);
                }
                if (instance->config.auto_key) {
                    json->Key("auto_key"); json->String(instance->config.auto_key);
                }
                json->Key("allow_guests"); json->Bool(instance->config.allow_guests);
                if (instance->config.export_days) {
                    json->Key("export_days"); json->Int(instance->config.export_days);
                } else {
                    json->Key("export_days"); json->Null();
                }
                json->Key("export_time"); json->Int(instance->config.export_time);
                json->Key("export_all"); json->Bool(instance->config.export_all);
                json->Key("fs_version"); json->Int64(instance->fs_version);
            json->EndObject();

            json->EndObject();
        }

        json->EndArray();
    });
}

void HandleInstanceAssign(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to delete users");
        io->SendError(403);
        return;
    }

    int64_t userid = -1;
    const char *instance = nullptr;
    uint32_t permissions = UINT32_MAX;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "userid") {
                parser.ParseInt(&userid);
            } else if (key == "instance") {
                parser.ParseString(&instance);
            } else if (key == "permissions") {
                if (!parser.SkipNull()) {
                    permissions = 0;

                    parser.ParseArray();
                    while (parser.InArray()) {
                        Span<const char> str = {};
                        parser.ParseString(&str);

                        char perm[128];
                        json_ConvertFromJsonName(str, perm);

                        if (!OptionToFlagI(UserPermissionNames, perm, &permissions)) {
                            LogError("Unknown permission '%1'", str);
                            io->SendError(422);
                            return;
                        }
                    }
                }
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

    // Check missing values
    {
        bool valid = true;

        if (userid < 0) {
            LogError("Missing or invalid 'userid' parameter");
            valid = false;
        }
        if (!instance) {
            LogError("Missing 'instance' parameter");
            valid = false;
        }
        if (permissions == UINT32_MAX) {
            LogError("Missing 'permissions' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Does instance exist?
    {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?1", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, instance, -1, SQLITE_STATIC);

        if (stmt.Step() && !session->IsRoot()) {
            Span<const char> master = SplitStr(instance, '/');

            if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                         WHERE userid = ?1 AND instance = ?2 AND
                                               permissions & ?3)", &stmt))
                return;
            sqlite3_bind_int64(stmt, 1, session->userid);
            sqlite3_bind_text(stmt, 2, master.ptr, (int)master.len, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

            stmt.Step();
        }

        if (!stmt.IsRow()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance);
                io->SendError(404);
            }
            return;
        }
    }

    // Does user exist?
    const char *username;
    {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare("SELECT root, username FROM dom_users WHERE userid = ?1", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, userid);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
            }
            return;
        }

        if (!session->IsRoot()) {
            bool is_root = (sqlite3_column_int(stmt, 0) == 1);

            if (is_root) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
                return;
            }
        }

        username = DuplicateString((const char *)sqlite3_column_text(stmt, 1), io->Allocator()).ptr;
    }

    gp_domain.db.Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5 || '+' || ?6 || ':' || ?7))",
                              time, request.client_addr, "assign_user", session->username,
                              instance, username, permissions))
            return false;

        // Adjust permissions
        if (permissions) {
            if (!gp_domain.db.Run(R"(INSERT INTO dom_permissions (instance, userid, permissions)
                                     VALUES (?1, ?2, ?3)
                                     ON CONFLICT (instance, userid) DO UPDATE SET permissions = excluded.permissions)",
                                  instance, userid, permissions))
                return false;
        } else {
            if (!gp_domain.db.Run("DELETE FROM dom_permissions WHERE instance = ?1 AND userid = ?2",
                                  instance, userid))
                return false;
        }

        InvalidateUserStamps(userid);

        io->SendText(200, "{}", "application/json");
        return true;
    });
}

void HandleInstancePermissions(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to list users");
        io->SendError(403);
        return;
    }

    const char *instance_key = request.GetQueryValue("instance");
    if (!instance_key) {
        LogError("Missing 'instance' parameter");
        io->SendError(422);
        return;
    }

    InstanceHolder *instance = gp_domain.Ref(instance_key);
    if (!instance) {
        LogError("Instance '%1' does not exist", instance_key);
        io->SendError(404);
        return;
    }
    K_DEFER { instance->Unref(); };

    // Can this admin user touch this instance?
    if (!session->IsRoot()) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND instance = ?2 AND
                                           permissions & ?3)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_text(stmt, 2, instance->master->key.ptr, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance_key);
                io->SendError(404);
            }
            return;
        }
    }

    sq_Statement stmt;
    if (!gp_domain.db.Prepare(R"(SELECT p.userid, p.permissions, u.root
                                 FROM dom_permissions p
                                 INNER JOIN dom_users u ON (u.userid = p.userid)
                                 WHERE p.instance = ?1
                                 ORDER BY p.instance)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, instance_key, -1, SQLITE_STATIC);

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        while (stmt.Step()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            uint32_t permissions = (uint32_t)sqlite3_column_int64(stmt, 1);
            bool is_root = (sqlite3_column_int(stmt, 2) == 1);
            char buf[128];

            if (is_root && !session->IsRoot())
                continue;

            if (instance->master != instance) {
                permissions &= UserPermissionSlaveMask;
            } else if (instance->slaves.len) {
                permissions &= UserPermissionMasterMask;
            }
            if (!permissions)
                continue;

            json->Key(Fmt(buf, "%1", userid).ptr); json->StartArray();
            for (Size i = 0; i < K_LEN(UserPermissionNames); i++) {
                if (instance->legacy && !(LegacyPermissionMask & (1 << i)))
                    continue;

                if (permissions & (1 << i)) {
                    Span<const char> str = json_ConvertToJsonName(UserPermissionNames[i], buf);
                    json->String(str.ptr, (size_t)str.len);
                }
            }
            json->EndArray();
        }
        if (!stmt.IsValid())
            return;

        json->EndObject();
    });
}

void HandleInstanceMigrate(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to list users");
        io->SendError(403);
        return;
    }

    const char *instance_key = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "instance") {
                parser.ParseString(&instance_key);
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

    // Check missing values
    if (!instance_key) {
        LogError("Missing 'instance' parameter");
        io->SendError(422);
        return;
    }

    InstanceHolder *instance = gp_domain.Ref(instance_key);
    if (!instance) {
        LogError("Instance '%1' does not exist", instance_key);
        io->SendError(404);
        return;
    }
    K_DEFER_N(ref_guard) { instance->Unref(); };

    // Can this admin user touch this instance?
    if (!session->IsRoot()) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT instance FROM dom_permissions
                                     WHERE userid = ?1 AND instance = ?2 AND
                                           permissions & ?3)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);
        sqlite3_bind_text(stmt, 2, instance->master->key.ptr, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Instance '%1' does not exist", instance_key);
                io->SendError(404);
            }
            return;
        }
    }

    // Make sure it is a legacy instance
    if (!instance->legacy) {
        LogError("Instance '%1' is not legacy", instance_key);
        io->SendError(422);
        return;
    }

    // Migration can take a long time, don't timeout because request looks idle
    io->ExtendTimeout(120000);

    // Be safe...
    {
        bool conflict;
        if (!ArchiveInstances(instance, &conflict)) {
            if (conflict) {
                io->SendError(409, "Archive already exists");
            }
            return;
        }
    }

    if (!MigrateInstance(instance->db, InstanceVersion))
        return;

    // Don't use instance after that!
    instance->Unref();
    instance = nullptr;
    ref_guard.Disable();

    if (!gp_domain.SyncInstance(instance_key))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleArchiveCreate(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to create archives");
        io->SendError(403);
        return;
    }

    // Can take a long time, don't timeout because request looks idle
    io->ExtendTimeout(60000);

    // Do the work
    {
        bool conflict;
        if (!ArchiveInstances(nullptr, &conflict)) {
            if (conflict) {
                io->SendError(409, "Archive already exists");
            }
            return;
        }
    }

    io->SendText(200, "{}", "application/json");
}

void HandleArchiveDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to delete archives");
        io->SendError(403);
        return;
    }

    const char *basename = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "filename") {
                parser.ParseString(&basename);
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

    // Check missing values
    if (!basename) {
        LogError("Missing 'filename' parameter");
        io->SendError(422);
        return;
    }

    // Safety checks
    if (PathIsAbsolute(basename)) {
        LogError("Path must not be absolute");
        io->SendError(403);
        return;
    }
    if (PathContainsDotDot(basename)) {
        LogError("Path must not contain any '..' component");
        io->SendError(403);
        return;
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.archive_directory, basename).ptr;

    if (!TestFile(filename, FileType::File)) {
        io->SendError(404);
        return;
    }
    if (!UnlinkFile(filename))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleArchiveList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Root user needs to confirm identity");
        io->SendError(401);
        return;
    }

    HeapArray<const char *> filenames;
    HeapArray<FileInfo> infos;
    {
        EnumResult ret = EnumerateDirectory(gp_domain.config.archive_directory, nullptr, -1,
                                            [&](const char *basename, FileType) {
            Span<const char> extension = GetPathExtension(basename);

            if (extension == ".goarch" || extension == ".goupilearchive") {
                const char *filename = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.archive_directory, basename).ptr;
                FileInfo file_info = {};

                // Go on even if this fails, or archive is in creation. Errors end up in the log anyway
                if (StatFile(filename, &file_info) != StatResult::Success)
                    return true;
                if (!file_info.size)
                    return true;

                const char *basename = SplitStrReverseAny(filename, K_PATH_SEPARATORS).ptr;

                filenames.Append(basename);
                infos.Append(file_info);
            }

            return true;
        });
        if (ret != EnumResult::Success)
            return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        for (Size i = 0; i < filenames.len; i++) {
            json->StartObject();
            json->Key("filename"); json->String(filenames[i]);
            json->Key("size"); json->Int64(infos[i].size);
            json->Key("mtime"); json->Int64(infos[i].mtime);
            json->EndObject();
        }

        json->EndArray();
    });
}

void HandleArchiveDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to download archives");
        io->SendError(403);
        return;
    }

    const char *basename = request.path + 26;

    // Safety checks
    if (!StartsWith(request.path, "/admin/api/archives/files/")) {
        LogError("Malformed or missing filename");
        io->SendError(422);
        return;
    }
    if (!basename[0] || strpbrk(basename, K_PATH_SEPARATORS)) {
        LogError("Filename cannot be empty or contain path separators");
        io->SendError(422);
        return;
    }
    if (GetPathExtension(basename) != ".goarch" &&
            GetPathExtension(basename) != ".goupilearchive") {
        LogError("Path must end with '.goarch' or '.goupilearchive' extension");
        io->SendError(403);
        return;
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.archive_directory, basename).ptr;
    const char *disposition = Fmt(io->Allocator(), "attachment; filename=\"%1\"", basename).ptr;

    io->AddHeader("Content-Disposition", disposition);
    io->SendFile(200, filename);
}

void HandleArchiveUpload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to upload archives");
        io->SendError(403);
        return;
    }

    const char *basename = request.path + 26;

    if (!StartsWith(request.path, "/admin/api/archives/files/")) {
        LogError("Malformed or missing filename");
        io->SendError(422);
        return;
    }
    if (!basename[0] || strpbrk(basename, K_PATH_SEPARATORS)) {
        LogError("Filename cannot be empty or contain path separators");
        io->SendError(422);
        return;
    }
    if (GetPathExtension(basename) != ".goarch" &&
            GetPathExtension(basename) != ".goupilearchive") {
        LogError("Path must end with '.goarch' or '.goupilearchive' extension");
        io->SendError(403);
        return;
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.archive_directory, basename).ptr;

    StreamWriter writer;
    if (!writer.Open(filename, (int)StreamWriterFlag::Exclusive |
                               (int)StreamWriterFlag::Atomic)) {
        if (errno == EEXIST) {
            LogError("An archive already exists with this name");
            io->SendError(409);
        }
        return;
    }

    StreamReader reader;
    if (!io->OpenForRead(Megabytes(512), &reader))
        return;

    // Read and store
    do {
        LocalArray<uint8_t, 16384> buf;
        buf.len = reader.Read(buf.data);
        if (buf.len < 0)
            return;

        if (!writer.Write(buf))
            return;
    } while (!reader.IsEOF());

    if (!writer.Close())
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleArchiveRestore(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsRoot()) {
        LogError("Non-root users are not allowed to upload archives");
        io->SendError(403);
        return;
    }

    const char *basename = nullptr;
    const char *decrypt_key = nullptr;
    bool restore_users = false;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "filename") {
                parser.ParseString(&basename);
            } else if (key == "key") {
                parser.ParseString(&decrypt_key);
            } else if (key == "users") {
                parser.ParseBool(&restore_users);
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

    // Check missing values
    {
        bool valid = true;

        if (!basename) {
            LogError("Missing 'filename' parameter");
            valid = false;
        }
        if (!decrypt_key) {
            LogError("Missing 'key' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Safety checks
    if (PathIsAbsolute(basename)) {
        LogError("Path must not be absolute");
        io->SendError(403);
        return;
    }
    if (PathContainsDotDot(basename)) {
        LogError("Path must not contain any '..' component");
        io->SendError(403);
        return;
    }
    if (GetPathExtension(basename) != ".goarch" &&
            GetPathExtension(basename) != ".goupilearchive") {
        LogError("Path must end with '.goarch' or '.goupilearchive' extension");
        io->SendError(403);
        return;
    }

    // Create directory for instance files
    const char *tmp_directory = CreateUniqueDirectory(gp_domain.config.tmp_directory, nullptr, io->Allocator());
    HeapArray<const char *> tmp_filenames;
    K_DEFER {
        for (const char *filename: tmp_filenames) {
            UnlinkFile(filename);
        }
        if (tmp_directory) {
            UnlinkDirectory(tmp_directory);
        }
    };

    // Extract archive to unencrypted ZIP file
    const char *extract_filename;
    {
        const char *src_filename = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.archive_directory, basename).ptr;

        int fd = -1;
        extract_filename = CreateUniqueFile(gp_domain.config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
        if (!extract_filename)
            return;
        tmp_filenames.Append(extract_filename);
        K_DEFER { CloseDescriptor(fd); };

        StreamReader reader(src_filename);
        StreamWriter writer(fd, extract_filename);
        if (!reader.IsValid()) {
            if (errno == ENOENT) {
                LogError("Archive '%1' does not exist", basename);
                io->SendError(404);
            }
            return;
        }
        if (!writer.IsValid())
            return;

        UnsealResult ret = UnsealArchive(&reader, &writer, decrypt_key);
        if (ret != UnsealResult::Success) {
            if (reader.IsValid()) {
                io->SendError(ret == UnsealResult::WrongKey ? 403 : 422);
            }
            return;
        }
        if (!writer.Close())
            return;
    }

    // Open ZIP file
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_reader_init_file(&zip, extract_filename, 0)) {
        LogError("Failed to open ZIP archive: %1", mz_zip_get_error_string(zip.m_last_error));
        return;
    }
    K_DEFER { mz_zip_reader_end(&zip); };

    // Extract and open archived main database (goupile.db)
    sq_Database main_db;
    {
        int fd = -1;
        const char *main_filename = CreateUniqueFile(gp_domain.config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
        if (!main_filename)
            return;
        tmp_filenames.Append(main_filename);
        K_DEFER { CloseDescriptor(fd); };

        int success = mz_zip_reader_extract_file_to_callback(&zip, "goupile.db", [](void *udata, mz_uint64, const void *ptr, size_t len) {
            K_ASSERT(len <= K_SIZE_MAX);

            int fd = *(int *)udata;
            Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)len);

            while (buf.len) {
#if defined(_WIN32)
                Size write_len = _write(fd, buf.ptr, (unsigned int)buf.len);
#else
                Size write_len = K_RESTART_EINTR(write(fd, buf.ptr, (size_t)buf.len), < 0);
#endif
                if (write_len < 0) {
                    LogError("Failed to write to ZIP: %1", strerror(errno));
                    return (size_t)0;
                }

                buf.ptr += write_len;
                buf.len -= write_len;
            }

            return len;
        }, &fd, 0);
        if (!success) {
            if (zip.m_last_error != MZ_ZIP_WRITE_CALLBACK_FAILED) {
                LogError("Failed to extract 'goupile.db' from archive: %1", mz_zip_get_error_string(zip.m_last_error));
            }
            return;
        }

        if (!main_db.Open(main_filename, SQLITE_OPEN_READWRITE))
            return;
        if (!MigrateDomain(&main_db, nullptr))
            return;
    }

    struct RestoreEntry {
        const char *key;
        const char *basename;
        const char *filename;
    };

    // Gather information from goupile.db
    HeapArray<RestoreEntry> entries;
    {
        sq_Statement stmt;
        if (!main_db.Prepare("SELECT instance, master FROM dom_instances ORDER BY instance", &stmt))
            return;

        while (stmt.Step()) {
            const char *instance_key = (const char *)sqlite3_column_text(stmt, 0);

            RestoreEntry entry = {};

            entry.key = DuplicateString(instance_key, io->Allocator()).ptr;
            entry.basename = MakeInstanceFileName("instances", instance_key, io->Allocator());
#if defined(_WIN32)
            for (Size i = 0; entry.basename[i]; i++) {
                char *ptr = (char *)entry.basename;
                int c = entry.basename[i];

                ptr[i] = (char)(c == '\\' ? '/' : c);
            }
#endif
            entry.filename = MakeInstanceFileName(tmp_directory, instance_key, io->Allocator());

            entries.Append(entry);
            tmp_filenames.Append(entry.filename);
        }
        if (!stmt.IsValid())
            return;
    }

    // Extract and migrate individual database files
    for (const RestoreEntry &entry: entries) {
        if (!mz_zip_reader_extract_file_to_file(&zip, entry.basename, entry.filename, 0)) {
            LogError("Failed to extract '%1' from archive: %2", entry.basename, mz_zip_get_error_string(zip.m_last_error));
            return;
        }

        if (!MigrateInstance(entry.filename))
            return;
    }

    // Save current instances
    {
        bool conflict;
        if (!ArchiveInstances(nullptr, &conflict)) {
            if (conflict) {
                io->SendError(409, "Archive already exists");
            }
            return;
        }
    }

    // Prepare for cleanup up of old instance directory
    const char *swap_directory = nullptr;
    K_DEFER {
        if (swap_directory) {
            EnumerateDirectory(swap_directory, nullptr, -1, [&](const char *filename, FileType) {
                filename = Fmt(io->Allocator(), "%1%/%2", swap_directory, filename).ptr;
                UnlinkFile(filename);

                return true;
            });
            UnlinkDirectory(swap_directory);
        }
    };

    // Replace running instances
    bool success = gp_domain.db.Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "restore", session->username,
                              basename))
            return false;

        if (!gp_domain.db.Run("DELETE FROM dom_instances"))
            return false;
        if (!gp_domain.SyncAll())
            return false;
        for (const RestoreEntry &entry: entries) {
            if (!gp_domain.db.Run("INSERT INTO dom_instances (instance) VALUES (?1)", entry.key))
                return false;
        }

        // It would be much better to do this by ATTACHing the old database and do the copy
        // in SQL. Unfortunately this triggers memory problems in SQLite Multiple Ciphers and
        // I don't have time to investigate this right now.
        if (restore_users) {
            bool success = gp_domain.db.RunMany(R"(
                DELETE FROM dom_permissions;
                DELETE FROM dom_users;
                DELETE FROM sqlite_sequence WHERE name = 'dom_users';
            )");
            if (!success)
                return false;

            // Copy users
            {
                sq_Statement stmt;
                if (!main_db.Prepare(R"(SELECT userid, username, password_hash,
                                               root, local_key, email, phone
                                        FROM dom_users)", &stmt))
                    return false;

                while (stmt.Step()) {
                    int64_t userid = sqlite3_column_int64(stmt, 0);
                    const char *username = (const char *)sqlite3_column_text(stmt, 1);
                    const char *password_hash = (const char *)sqlite3_column_text(stmt, 2);
                    int root = sqlite3_column_int(stmt, 3);
                    const char *local_key = (const char *)sqlite3_column_text(stmt, 4);
                    const char *email = (const char *)sqlite3_column_text(stmt, 5);
                    const char *phone = (const char *)sqlite3_column_text(stmt, 6);

                    if (!gp_domain.db.Run(R"(INSERT INTO dom_users (userid, username, password_hash,
                                                                    change_password, root, local_key, email, phone)
                                             VALUES (?1, ?2, ?3, 0, ?4, ?5, ?6, ?7))",
                                          userid, username, password_hash, root, local_key, email, phone))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;
            }

            // Copy permissions
            {
                sq_Statement stmt;
                if (!main_db.Prepare("SELECT userid, instance, permissions FROM dom_permissions", &stmt))
                    return false;

                while (stmt.Step()) {
                    int64_t userid = sqlite3_column_int64(stmt, 0);
                    const char *instance_key = (const char *)sqlite3_column_text(stmt, 1);
                    int64_t permissions = sqlite3_column_int(stmt, 2);

                    if (!gp_domain.db.Run(R"(INSERT INTO dom_permissions (userid, instance, permissions)
                                             VALUES (?1, ?2, ?3))",
                                          userid, instance_key, permissions))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;
            }
        }

#if defined(__linux__) && defined(RENAME_EXCHANGE)
        if (renameat2(AT_FDCWD, gp_domain.config.instances_directory,
                      AT_FDCWD, tmp_directory, RENAME_EXCHANGE) < 0) {
            LogDebug("Failed to swap directories atomically: %1", strerror(errno));
#else
        if (true) {
#endif
            swap_directory = Fmt(io->Allocator(), "%1%/%2", gp_domain.config.tmp_directory, FmtRandom(24)).ptr;

            unsigned int flags = (int)RenameFlag::Overwrite | (int)RenameFlag::Sync;

            // Non atomic swap but it is hard to do better here
            if (RenameFile(gp_domain.config.instances_directory, swap_directory, flags) != RenameResult::Success)
                return false;
            if (RenameFile(tmp_directory, gp_domain.config.instances_directory, flags) != RenameResult::Success) {
                // If this goes wrong, we're completely screwed :)
                // At least on Linux we have some hope to avoid this problem
                RenameFile(swap_directory, gp_domain.config.instances_directory, flags);
                return false;
            }
        } else {
            swap_directory = tmp_directory;
        }

        K_ASSERT(tmp_filenames.len == entries.len + 2);
        tmp_filenames.RemoveFrom(2);
        tmp_directory = nullptr;

        return true;
    });
    if (!success)
        return;

    if (!gp_domain.SyncAll(true))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleUserCreate(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to create users");
        io->SendError(403);
        return;
    }

    const char *username = nullptr;
    const char *password = nullptr;
    bool change_password = true;
    bool confirm = false;
    const char *email = nullptr;
    const char *phone = nullptr;
    bool root = false;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "username") {
                parser.ParseString(&username);
            } else if (key == "password") {
                parser.ParseString(&password);
            } else if (key == "change_password") {
                parser.ParseBool(&change_password);
            } else if (key == "confirm") {
                parser.ParseBool(&confirm);
            } else if (key == "email") {
                parser.ParseString(&email);
                email = email[0] ? email : nullptr;
            } else if (key == "phone") {
                parser.ParseString(&phone);
                phone = phone[0] ? phone : nullptr;
            } else if (key == "root") {
                parser.ParseBool(&root);
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

        if (!username || !password) {
            LogError("Missing 'username' or 'password' parameter");
            valid = false;
        }
        if (username && !CheckUserName(username)) {
            valid = false;
        }
        if (password && strlen(password) < pwd_MinLength) {
            LogError("Password is too short");
            valid = false;
        }
        if (email && !strchr(email, '@')) {
            LogError("Invalid email address format");
            valid = false;
        }
        if (phone && phone[0] != '+') {
            LogError("Invalid phone number format (prefix is mandatory)");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Safety checks
    if (root && !session->IsRoot()) {
        LogError("You cannot create a root user");
        io->SendError(403);
        return;
    }

    // Hash password
    char hash[PasswordHashBytes];
    if (!HashPassword(password, hash))
        return;

    // Create local key
    char local_key[45];
    {
        uint8_t buf[32];
        FillRandomSafe(buf);
        sodium_bin2base64(local_key, K_SIZE(local_key), buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    gp_domain.db.Transaction([&]() {
        // Check for existing user
        {
            sq_Statement stmt;
            if (!gp_domain.db.Prepare("SELECT userid FROM dom_users WHERE username = ?1", &stmt))
                return false;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

            if (stmt.Step()) {
                LogError("User '%1' already exists", username);
                io->SendError(409);
                return false;
            } else if (!stmt.IsValid()) {
                return false;
            }
        }

        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "create_user", session->username,
                              username))
            return false;

        // Create user
        if (!gp_domain.db.Run(R"(INSERT INTO dom_users (username, password_hash, change_password,
                                                        email, phone, root, local_key, confirm)
                                 VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8))",
                              username, hash, 0 + change_password, email, phone, 0 + root, local_key,
                              confirm ? "TOTP" : nullptr))
            return false;

        io->SendText(200, "{}", "application/json");
        return true;
    });
}

void HandleUserEdit(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to edit users");
        io->SendError(403);
        return;
    }

    int64_t userid = -1;
    const char *username = nullptr;
    const char *password = nullptr;
    bool change_password = true;
    bool confirm = false, set_confirm = false;
    bool reset_secret = false;
    const char *email = nullptr; bool set_email = false;
    const char *phone = nullptr; bool set_phone = false;
    bool root = false, set_root = false;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "userid") {
                parser.ParseInt(&userid);
            } else if (key == "username") {
                parser.SkipNull() || parser.ParseString(&username);
            } else if (key == "password") {
                parser.SkipNull() || parser.ParseString(&password);
            } else if (key == "change_password") {
                parser.SkipNull() || parser.ParseBool(&change_password);
            } else if (key == "confirm") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&confirm);
                    set_confirm = true;
                }
            } else if (key == "reset_secret") {
                parser.SkipNull() || parser.ParseBool(&reset_secret);
            } else if (key == "email") {
                if (!parser.SkipNull()) {
                    parser.ParseString(&email);

                    email = email[0] ? email : nullptr;
                    set_email = true;
                }
            } else if (key == "phone") {
                if (!parser.SkipNull()) {
                    parser.ParseString(&phone);

                    phone = phone[0] ? phone : nullptr;
                    set_phone = true;
                }
            } else if (key == "root") {
                if (!parser.SkipNull()) {
                    parser.ParseBool(&root);
                    set_root = true;
                }
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

        if (userid < 0) {
            LogError("Missing or invalid 'userid' parameter");
            valid = false;
        }
        if (username && !CheckUserName(username)) {
            valid = false;
        }
        if (password && strlen(password) < pwd_MinLength) {
            LogError("Password is too short");
            valid = false;
        }
        if (email && !strchr(email, '@')) {
            LogError("Invalid email address format");
            valid = false;
        }
        if (phone && phone[0] != '+') {
            LogError("Invalid phone number format (prefix is mandatory)");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Safety checks
    if (root && !session->IsRoot()) {
        LogError("You cannot create a root user");
        io->SendError(403);
        return;
    }
    if (userid == session->userid && set_root && root != session->is_root) {
        LogError("You cannot change your root privileges");
        io->SendError(403);
        return;
    }

    // Hash password
    char hash[PasswordHashBytes];
    if (password && !HashPassword(password, hash))
        return;

    // Check for existing user
    {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare("SELECT root FROM dom_users WHERE userid = ?1", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, userid);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
            }
            return;
        }

        if (!session->IsRoot()) {
            bool is_root = (sqlite3_column_int(stmt, 0) == 1);

            if (is_root) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
                return;
            }
        }
    }

    gp_domain.db.Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              time, request.client_addr, "edit_user", session->username,
                              username))
            return false;

        // Edit user
        if (username && !gp_domain.db.Run("UPDATE dom_users SET username = ?2 WHERE userid = ?1", userid, username))
            return false;
        if (password && !gp_domain.db.Run("UPDATE dom_users SET password_hash = ?2 WHERE userid = ?1", userid, hash))
            return false;
        if (change_password && !gp_domain.db.Run("UPDATE dom_users SET change_password = ?2 WHERE userid = ?1", userid, 0 + change_password))
            return false;
        if (set_confirm && !gp_domain.db.Run("UPDATE dom_users SET confirm = ?2 WHERE userid = ?1", userid, confirm ? "TOTP" : nullptr))
            return false;
        if (reset_secret && !gp_domain.db.Run("UPDATE dom_users SET secret = NULL WHERE userid = ?1", userid))
            return false;
        if (set_email && !gp_domain.db.Run("UPDATE dom_users SET email = ?2 WHERE userid = ?1", userid, email))
            return false;
        if (set_phone && !gp_domain.db.Run("UPDATE dom_users SET phone = ?2 WHERE userid = ?1", userid, phone))
            return false;
        if (set_root && !gp_domain.db.Run("UPDATE dom_users SET root = ?2 WHERE userid = ?1", userid, 0 + root))
            return false;

        io->SendText(200, "{}", "application/json");
        return true;
    });
}

void HandleUserDelete(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to delete users");
        io->SendError(403);
        return;
    }

    int64_t userid = -1;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "userid") {
                parser.ParseInt(&userid);
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

    // Check missing values
    if (userid < 0) {
        LogError("Missing or invalid 'userid' parameter");
        io->SendError(422);
        return;
    }

    // Safety checks
    if (userid == session->userid) {
        LogError("You cannot delete yourself");
        io->SendError(403);
        return;
    }

    // Get user information
    const char *username;
    const char *local_key;
    {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare("SELECT username, local_key, root FROM dom_users WHERE userid = ?1", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, userid);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
            }
            return;
        }

        if (!session->IsRoot()) {
            bool is_root = (sqlite3_column_int(stmt, 2) == 1);

            if (is_root) {
                LogError("User ID '%1' does not exist", userid);
                io->SendError(404);
                return;
            }
        }

        username = DuplicateString((const char *)sqlite3_column_text(stmt, 0), io->Allocator()).ptr;
        local_key = DuplicateString((const char *)sqlite3_column_text(stmt, 1), io->Allocator()).ptr;
    }

    gp_domain.db.Transaction([&]() {
        // Log action
        int64_t time = GetUnixTime();
        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                 VALUES (?1, ?2, ?3, ?4, ?5 || ':' || ?6))",
                              time, request.client_addr, "delete_user", session->username,
                              username, local_key))
            return false;

        if (!gp_domain.db.Run("DELETE FROM dom_users WHERE userid = ?1", userid))
            return false;

        io->SendText(200, "{}", "application/json");
        return true;
    });
}

void HandleUserList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetAdminSession(io, nullptr);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->IsAdmin()) {
        LogError("Non-admin users are not allowed to list users");
        io->SendError(403);
        return;
    }

    sq_Statement stmt;
    if (!gp_domain.db.Prepare(R"(SELECT userid, username, email, phone, root, LOWER(confirm)
                                 FROM dom_users
                                 ORDER BY username)", &stmt))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt.Step()) {
            bool is_root = (sqlite3_column_int(stmt, 4) == 1);

            if (is_root && !session->IsRoot())
                continue;

            json->StartObject();
            json->Key("userid"); json->Int64(sqlite3_column_int64(stmt, 0));
            json->Key("username"); json->String((const char *)sqlite3_column_text(stmt, 1));
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
                json->Key("email"); json->String((const char *)sqlite3_column_text(stmt, 2));
            } else {
                json->Key("email"); json->Null();
            }
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                json->Key("phone"); json->String((const char *)sqlite3_column_text(stmt, 3));
            } else {
                json->Key("phone"); json->Null();
            }
            json->Key("root"); json->Bool(is_root);
            json->Key("confirm"); json->Bool(sqlite3_column_type(stmt, 5) != SQLITE_NULL);
            json->EndObject();
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

}
