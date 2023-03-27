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

class LocalDisk: public rk_Disk {
    int threads;

public:
    LocalDisk(const char *path, int threads);
    ~LocalDisk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    int GetThreads() const override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_obj) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) override;

    bool TestSlow(const char *path) override;
    bool TestFast(const char *path) override;
};

LocalDisk::LocalDisk(const char *path, int threads)
{
    if (threads > 0) {
        this->threads = threads;
    } else {
        this->threads = (GetCoreCount() + 1);
    }

    Span<const char> directory = NormalizePath(path, GetWorkingDirectory(), &str_alloc);

    // Sanity checks
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return;
    }

    url = directory.ptr;
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::Init(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    BlockAllocator temp_alloc;

    HeapArray<const char *> directories;
    RG_DEFER_N(err_guard) {
        for (Size i = directories.len - 1; i >= 0; i--) {
            const char *dirname = directories[i];
            UnlinkDirectory(dirname);
        }
    };

    // Create main directory
    if (TestFile(url)) {
        if (!IsDirectoryEmpty(url)) {
            LogError("Directory '%1' exists and is not empty", url);
            return false;
        }
    } else {
        if (!MakeDirectory(url))
            return false;
        directories.Append(url);
    }
    if (!MakeDirectory(url, false))
        return false;

    // Init subdirectories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1%/%2", url, suffix).ptr;

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

        for (int i = 0; i < 4096; i++) {
            char name[128];
            Fmt(name, "blobs/%1", FmtHex(i).Pad0(-3));

            if (!make_directory(name))
                return false;
        }
    }

    if (!InitKeys(full_pwd, write_pwd))
        return false;

    err_guard.Disable();
    return true;
}

int LocalDisk::GetThreads() const
{
    return threads;
}

Size LocalDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, out_buf);
}

Size LocalDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_obj)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, Mebibytes(256), out_obj);
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
            LogError("Failed to create temporary file in '%1'", tmp);
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

bool LocalDisk::DeleteRaw(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return UnlinkFile(filename.data);
}

bool LocalDisk::ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths)
{
    Size prev_len = out_paths->len;
    Size url_len = strlen(url);

    LocalArray<char, MaxPathSize + 128> dirname;
    dirname.len = Fmt(dirname.data, "%1%/%2", url, path).len;

    if (!EnumerateFiles(dirname.data, nullptr, 0, -1, alloc, out_paths))
        return false;

    for (Size i = prev_len; i < out_paths->len; i++) {
        out_paths->ptr[i] += url_len + 1;
    }

    return true;
}

bool LocalDisk::TestSlow(const char *path)
{
    bool exists = TestFile(path, FileType::File);
    return exists;
}

bool LocalDisk::TestFast(const char *path)
{
    bool exists = TestFile(path, FileType::File);
    return exists;
}

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path, const char *pwd, int threads)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<LocalDisk>(path, threads);

    if (!disk->GetURL())
        return nullptr;
    if (pwd && !disk->Open(pwd))
        return nullptr;

    return disk;
}

}
