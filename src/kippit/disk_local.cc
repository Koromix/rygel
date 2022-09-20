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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "disk.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int MaxPathSize = 4096 - 128;
static const uint8_t RepoVersion = 1;

class LocalDisk: public kt_Disk {
public:
    LocalDisk(Span<const char> directory, kt_DiskMode mode, const uint8_t skey[32], const uint8_t pkey[32]);
    ~LocalDisk() override;

    bool ReadObject(const char *path, HeapArray<uint8_t> *out_obj) override;
    Size WriteObject(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
};

LocalDisk::LocalDisk(Span<const char> directory, kt_DiskMode mode, const uint8_t skey[32], const uint8_t pkey[32])
{
    RG_ASSERT(directory.len <= MaxPathSize);

    this->url = NormalizePath(directory, GetWorkingDirectory(), &str_alloc).ptr;

    this->mode = mode;
    memcpy(this->skey, skey, RG_SIZE(this->skey));
    memcpy(this->pkey, pkey, RG_SIZE(this->pkey));
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::ReadObject(const char *path, HeapArray<uint8_t> *out_obj)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, Mebibytes(16), out_obj) >= 0;
}

Size LocalDisk::WriteObject(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    // Open destination file
    FILE *fp;
    {
        bool exists = false;
        fp = OpenFile(filename.data, (int)OpenFlag::Write | (int)OpenFlag::Exclusive, &exists);
        if (!fp)
            return exists ? 0 : -1;
    }
    RG_DEFER { fclose(fp); };

    StreamWriter writer(fp, filename.data);

    // Write encrypted content
    if (!func([&](Span<const uint8_t> buf) { return writer.Write(buf); }))
        return -1;
    if (!writer.Close())
        return -1;

    return writer.GetRawWritten();
}

static bool DeriveKey(const char *pwd, const uint8_t salt[16], uint8_t out_key[32])
{
    RG_STATIC_ASSERT(crypto_pwhash_SALTBYTES == 16);

    if (crypto_pwhash(out_key, 32, pwd, strlen(pwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        LogError("Failed to derive key from password (exhausted resource?)");
        return false;
    }

    return true;
}

static bool WriteControl(const char *filename, const uint8_t key[32], Span<const uint8_t> data)
{
    uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + Kibibytes(16)];
    RG_ASSERT(data.len <= Kibibytes(16));

    randombytes_buf(cypher, crypto_secretbox_NONCEBYTES);
    crypto_secretbox_easy(cypher + crypto_secretbox_NONCEBYTES, data.ptr, (size_t)data.len, cypher, key);

    Span<const uint8_t> buf = MakeSpan(cypher, crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + data.len);
    return WriteFile(buf, filename);
}

static bool ReadControl(const char *filename, const uint8_t key[32],
                        Span<uint8_t> out_data, bool *out_error = nullptr)
{
    uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + Kibibytes(16)];
    RG_ASSERT(out_data.len <= Kibibytes(16));

    Size len = ReadFile(filename, cypher);
    if (len < 0) {
        if (out_error) {
            *out_error = true;
        }
        return false;
    }

    if (key) {
        if (len - crypto_secretbox_NONCEBYTES - crypto_secretbox_MACBYTES != out_data.len) {
            LogError("Unexpected size for '%1'", filename);

            if (out_error) {
                *out_error = true;
            }
            return false;
        }
        if (crypto_secretbox_open_easy(out_data.ptr, cypher + crypto_secretbox_NONCEBYTES,
                                       len - crypto_secretbox_NONCEBYTES, cypher, key) != 0) {
            if (!out_error) {
                LogError("Failed to decrypt control file");
            }
            return false;
        }
    } else {
        if (len != out_data.len) {
            LogError("Unexpected size for '%1'", filename);

            if (out_error) {
                *out_error = true;
            }
            return false;
        }

        memcpy_safe(out_data.ptr, cypher, len);
    }

    return true;
}

bool kt_CreateLocalDisk(const char *path, const char *full_pwd, const char *write_pwd)
{
    BlockAllocator temp_alloc;

    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    // Sanity checks
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return false;
    }

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    RG_DEFER_N(root_guard) {
        for (const char *filename: files) {
            UnlinkFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            UnlinkDirectory(directories[i]);
        }
    };

    // Make main directory
    if (!MakeDirectory(path))
        return false;
    directories.Append(path);

    // Create repository directories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1%/%2", directory, suffix).ptr;

            if (!MakeDirectory(path))
                return false;
            directories.Append(path);

            return true;
        };

        if (!make_directory("info"))
            return false;
        if (!make_directory("blobs"))
            return false;

        for (int i = 0; i < 256; i++) {
            char name[128];
            Fmt(name, "blobs/%1", FmtHex(i).Pad0(-2));

            if (!make_directory(name))
                return false;
        }
    }

    // Repository salt
    uint8_t salt[16];
    randombytes_buf(salt, RG_SIZE(salt));

    // Derive password keys
    uint8_t full_key[32];
    uint8_t write_key[32];
    if (!DeriveKey(full_pwd, salt, full_key))
        return false;
    if (!DeriveKey(write_pwd, salt, write_key))
        return false;

    // Generate secure keypair
    uint8_t skey[32];
    uint8_t pkey[32];
    crypto_box_keypair(pkey, skey);

    // Write control files
    {
        const char *salt_filename = Fmt(&temp_alloc, "%1%/info/salt", directory).ptr;
        const char *version_filename = Fmt(&temp_alloc, "%1%/info/version", directory).ptr;
        const char *full_filename = Fmt(&temp_alloc, "%1%/info/full", directory).ptr;
        const char *write_filename = Fmt(&temp_alloc, "%1%/info/write", directory).ptr;

        if (!WriteFile(salt, salt_filename))
            return false;
        files.Append(salt_filename);
        if (!WriteControl(version_filename, pkey, RepoVersion))
            return false;
        files.Append(version_filename);
        if (!WriteControl(full_filename, full_key, skey))
            return false;
        files.Append(full_filename);
        if (!WriteControl(write_filename, write_key, pkey))
            return false;
        files.Append(write_filename);
    }

    root_guard.Disable();
    return true;
}

kt_Disk *kt_OpenLocalDisk(const char *path, const char *pwd)
{
    BlockAllocator temp_alloc;

    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    // Sanity checks
    if (!TestFile(path, FileType::Directory)) {
        LogError("Directory '%1' does not exist", directory);
        return nullptr;
    }
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return nullptr;
    }

    const char *salt_filename = Fmt(&temp_alloc, "%1%/info/salt", directory).ptr;
    const char *version_filename = Fmt(&temp_alloc, "%1%/info/version", directory).ptr;
    const char *full_filename = Fmt(&temp_alloc, "%1%/info/full", directory).ptr;
    const char *write_filename = Fmt(&temp_alloc, "%1%/info/write", directory).ptr;

    uint8_t key[32];
    {
        uint8_t salt[16];
        if (!ReadControl(salt_filename, nullptr, salt))
            return nullptr;

        if (!DeriveKey(pwd, salt, key))
            return nullptr;
    }

    // Open disk and determine mode
    kt_DiskMode mode;
    uint8_t skey[32];
    uint8_t pkey[32];
    {
        bool error = false;

        if (ReadControl(write_filename, key, pkey, &error)) {
            mode = kt_DiskMode::WriteOnly;
            memset(skey, 0, RG_SIZE(key));
        } else if (ReadControl(full_filename, key, skey, &error)) {
            mode = kt_DiskMode::ReadWrite;
            crypto_scalarmult_base(pkey, skey);
        } else {
            if (!error) {
                LogError("Failed to open repository (wrong password?)");
            }
            return nullptr;
        }
    }

    // Read encrypted version file
    uint8_t test[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + 1];
    {
        const char *version_filename = Fmt(&temp_alloc, "%1%/info/version", directory).ptr;

        Size read = ReadFile(version_filename, test);
        if (read < 0)
            return nullptr;
        if (read < RG_SIZE(test)) {
            LogError("Truncated version file");
            return nullptr;
        }
    }

    // Check version
    {
        uint8_t version = 0;

        if (!ReadControl(version_filename, pkey, version))
            return nullptr;

        if (version != RepoVersion) {
            LogError("Unexpected repository version %1 (expected %2)", version, RepoVersion);
            return nullptr;
        }
    }

    kt_Disk *disk = new LocalDisk(directory, mode, skey, pkey);
    return disk;
}

}
