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
#include "disk.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int MaxPathSize = 4096 - 128;

class LocalDisk: public rk_Disk {
public:
    LocalDisk(const char *path, const rk_OpenSettings &settings);
    ~LocalDisk() override;

    bool Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_buf) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, FunctionRef<bool(const char *, int64_t)> func) override;
    StatResult TestRaw(const char *path, int64_t *out_size = nullptr) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;
};

LocalDisk::LocalDisk(const char *path, const rk_OpenSettings &settings)
    : rk_Disk(settings, std::min(2 * GetCoreCount(), 32))
{
    Span<const char> directory = NormalizePath(path, GetWorkingDirectory(), &str_alloc);

    // Sanity checks
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return;
    }

    // We're good!
    url = directory.ptr;
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users)
{
    RG_ASSERT(url);
    RG_ASSERT(!keyset);

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
        if (!make_directory("tmp"))
            return false;

        for (int i = 0; i < 256; i++) {
            char name[128];
            Fmt(name, "blobs/%1", FmtHex(i).Pad0(-2));

            if (!make_directory(name))
                return false;
        }
    }

    if (!InitDefault(mkey, users))
        return false;

    err_guard.Disable();
    return true;
}

Size LocalDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, out_buf);
}

Size LocalDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return ReadFile(filename.data, Mebibytes(256), out_buf);
}

Size LocalDisk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    // Create temporary file
    int fd = -1;
    LocalArray<char, MaxPathSize + 128> tmp;
    {
        tmp.len = Fmt(tmp.data, "%1%/tmp%/", url).len;

        for (int i = 0; i < 1000; i++) {
            Size len = Fmt(tmp.TakeAvailable(), "%1.tmp", FmtRandom(24)).len;

            OpenResult ret = OpenFile(tmp.data, (int)OpenFlag::Write | (int)OpenFlag::Exclusive,
                                                (int)OpenResult::FileExists, &fd);

            if (ret == OpenResult::Success) [[likely]] {
                tmp.len += len;
                break;
            } else if (ret != OpenResult::FileExists) {
                return -1;
            }
        }

        if (fd < 0) [[unlikely]] {
            LogError("Failed to create temporary file in '%1'", tmp);
            return -1;
        }
    }
    RG_DEFER_N(file_guard) { CloseDescriptor(fd); };
    RG_DEFER_N(tmp_guard) { UnlinkFile(tmp.data); };

    StreamWriter writer(fd, filename.data);

    // Write encrypted content
    if (!func([&](Span<const uint8_t> buf) { return writer.Write(buf); }))
        return -1;
    if (!writer.Close())
        return -1;

    // File is complete
    CloseDescriptor(fd);
    file_guard.Disable();

    // Atomic rename
    if (!RenameFile(tmp.data, filename.data, (int)RenameFlag::Overwrite))
        return -1;
    tmp_guard.Disable();

    int64_t written = writer.GetRawWritten();

    if (!PutCache(path, written))
        return -1;

    return written;
}

bool LocalDisk::DeleteRaw(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return UnlinkFile(filename.data);
}

bool LocalDisk::ListRaw(const char *path, FunctionRef<bool(const char *, int64_t)> func)
{
    BlockAllocator temp_alloc;

    path = path ? path : "";

    const char *dirname0 = path[0] ? Fmt(&temp_alloc, "%1/%2", url, path).ptr : url;
    Size url_len = strlen(url);

    HeapArray<const char *> pending_directories;
    pending_directories.Append(dirname0);

    for (Size i = 0; i < pending_directories.len; i++) {
        const char *dirname = pending_directories[i];

        EnumResult ret = EnumerateDirectory(dirname, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
            const char *filename = Fmt(&temp_alloc, "%1/%2", dirname, basename).ptr;
            const char *path = filename + url_len + 1;

            switch (file_info.type) {
                case FileType::Directory: {
                    if (TestStr(path, "tmp"))
                        return true;
                    pending_directories.Append(filename);
                } break;

                case FileType::File:
                case FileType::Link: {
                    if (!func(path, file_info.size))
                        return false;
                } break;

                case FileType::Device:
                case FileType::Pipe:
                case FileType::Socket: {} break;
            }

            return true;
        });
        if (ret != EnumResult::Success && ret != EnumResult::PartialEnum)
            return false;
    }

    return true;
}

StatResult LocalDisk::TestRaw(const char *path, int64_t *out_size)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    FileInfo file_info;
    StatResult ret = StatFile(filename.data, (int)StatFlag::SilentMissing, &file_info);

    if (ret == StatResult::Success && file_info.type != FileType::File) {
        LogError("Path '%1' is not a file", filename);
        return StatResult::OtherError;
    }

    if (out_size) {
        *out_size = file_info.size;
    }
    return ret;
}

bool LocalDisk::CreateDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return MakeDirectory(filename.data, false);
}

bool LocalDisk::DeleteDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return UnlinkDirectory(filename.data);
}

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path, const char *username, const char *pwd, const rk_OpenSettings &settings)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<LocalDisk>(path, settings);

    if (!disk->GetURL())
        return nullptr;
    if (username && !disk->Authenticate(username, pwd))
        return nullptr;

    return disk;
}

}
