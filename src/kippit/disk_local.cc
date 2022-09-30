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

class LocalDisk: public kt_Disk {
public:
    LocalDisk(Span<const char> directory, kt_DiskMode mode, const uint8_t skey[32], const uint8_t pkey[32]);
    ~LocalDisk() override;

    bool ReadRaw(const char *path, HeapArray<uint8_t> *out_obj) override;
    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
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

bool LocalDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_obj)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, Mebibytes(16), out_obj) >= 0;
}

Size LocalDisk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    if (TestFile(filename.data, FileType::File))
        return 0;

    // Create temporary file
    FILE *fp;
    LocalArray<char, MaxPathSize + 128> tmp;
    {
        tmp.len = Fmt(tmp.data, "%1%/", url).len;

        OpenResult ret = OpenResult::OtherError;

        for (int i = 0; i < 1000; i++) {
            Size len = Fmt(tmp.TakeAvailable(), "%1.tmp", FmtRandom(24)).len;

            ret = OpenFile(tmp.data, (int)OpenFlag::Write | (int)OpenFlag::Exclusive,
                                     (int)OpenResult::FileExists, &fp);

            if (RG_LIKELY(ret == OpenResult::Success)) {
                tmp.len += len;
                break;
            } else if (ret != OpenResult::FileExists) {
                return -1;
            }
        }
        if (RG_UNLIKELY(ret == OpenResult::FileExists)) {
            LogError("Failed to create temporoary file in '%1'", tmp);
            return -1;
        }

        RG_ASSERT(ret == OpenResult::Success);
    }
    RG_DEFER_N(file_guard) { fclose(fp); };
    RG_DEFER_N(tmp_guard) { UnlinkFile(tmp.data); };

    StreamWriter writer(fp, filename.data);

    // Write encrypted content
    if (!func([&](Span<const uint8_t> buf) { return writer.Write(buf); }))
        return -1;
    if (!writer.Close())
        return -1;

    // File is complete
    fclose(fp);
    file_guard.Disable();

    // Atomic rename
    if (!RenameFile(tmp.data, filename.data, (int)RenameFlag::Overwrite))
        return -1;
    tmp_guard.Disable();

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

#pragma pack(push, 1)
struct KeyData {
    uint8_t salt[16];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t cypher[crypto_secretbox_MACBYTES + 32];
};
#pragma pack(pop)

static bool WriteKey(const char *filename, const char *pwd, const uint8_t payload[32])
{
    KeyData data;

    randombytes_buf(data.salt, RG_SIZE(data.salt));
    randombytes_buf(data.nonce, RG_SIZE(data.nonce));

    uint8_t key[32];
    if (!DeriveKey(pwd, data.salt, key))
        return false;

    crypto_secretbox_easy(data.cypher, payload, 32, data.nonce, key);

    Span<const uint8_t> buf = MakeSpan((const uint8_t *)&data, RG_SIZE(data));
    return WriteFile(buf, filename);
}

static bool ReadKey(const char *filename, const char *pwd, uint8_t *out_payload, bool *out_error)
{
    KeyData data;

    // Read file data
    {
        Span<uint8_t> buf = MakeSpan((uint8_t *)&data, RG_SIZE(data));

        if (!ReadFile(filename, buf)) {
            *out_error = true;
            return false;
        }
    }

    uint8_t key[32];
    if (!DeriveKey(pwd, data.salt, key)) {
        *out_error = true;
        return false;
    }

    bool success = !crypto_secretbox_open_easy(out_payload, data.cypher, RG_SIZE(data.cypher), data.nonce, key);
    return success;
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
    if (TestFile(path)) {
        if (!IsDirectoryEmpty(path)) {
            LogError("Directory '%1' exists and is not empty", path);
            return false;
        }
    } else {
        if (!MakeDirectory(path))
            return false;
        directories.Append(path);
    }
    if (!MakeDirectory(path, false))
        return false;

    // Create repository directories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1%/%2", directory, suffix).ptr;

            if (!MakeDirectory(path))
                return false;
            directories.Append(path);

            return true;
        };

        if (!make_directory("keys"))
            return false;
        if (!make_directory("tags"))
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

    const char *full_filename = Fmt(&temp_alloc, "%1%/keys/full", directory).ptr;
    const char *write_filename = Fmt(&temp_alloc, "%1%/keys/write", directory).ptr;

    // Generate master keys
    uint8_t skey[32];
    uint8_t pkey[32];
    crypto_box_keypair(pkey, skey);

    // Write control files
    if (!WriteKey(full_filename, full_pwd, skey))
        return false;
    files.Append(full_filename);
    if (!WriteKey(write_filename, write_pwd, pkey))
        return false;
    files.Append(write_filename);

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

    const char *full_filename = Fmt(&temp_alloc, "%1%/keys/full", directory).ptr;
    const char *write_filename = Fmt(&temp_alloc, "%1%/keys/write", directory).ptr;

    // Open disk and determine mode
    kt_DiskMode mode;
    uint8_t skey[32];
    uint8_t pkey[32];
    {
        bool error = false;

        if (ReadKey(write_filename, pwd, pkey, &error)) {
            mode = kt_DiskMode::WriteOnly;
            memset(skey, 0, RG_SIZE(skey));
        } else if (ReadKey(full_filename, pwd, skey, &error)) {
            mode = kt_DiskMode::ReadWrite;
            crypto_scalarmult_base(pkey, skey);
        } else {
            if (!error) {
                LogError("Failed to open repository (wrong password?)");
            }
            return nullptr;
        }
    }

    kt_Disk *disk = new LocalDisk(directory, mode, skey, pkey);
    return disk;
}

}
