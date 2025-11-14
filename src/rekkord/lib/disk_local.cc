// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "disk.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static const int MaxPathSize = 4096 - 128;

class LocalDisk: public rk_Disk {
    BlockAllocator str_alloc;

public:
    LocalDisk(const char *path);
    ~LocalDisk() override;

    rk_ChecksumType GetChecksumType() override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;
    StatResult TestDirectory(const char *path) override;

    Size ReadFile(const char *path, Span<uint8_t> out_buf) override;
    Size ReadFile(const char *path, HeapArray<uint8_t> *out_buf) override;

    rk_WriteResult WriteFile(const char *path, Span<const uint8_t> buf, const rk_WriteSettings &settings = {}) override;
    bool DeleteFile(const char *path) override;
    bool RetainFile(const char *path, int64_t until) override;

    bool ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func) override;
    StatResult TestFile(const char *path, int64_t *out_size = nullptr) override;
};

LocalDisk::LocalDisk(const char *path)
{
    Span<const char> directory = NormalizePath(path, GetWorkingDirectory(), &str_alloc);

    // Sanity checks
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return;
    }

    // We're good!
    url = directory.ptr;
    default_threads = std::min(2 * GetCoreCount(), 32);
}

LocalDisk::~LocalDisk()
{
}

rk_ChecksumType LocalDisk::GetChecksumType()
{
    return rk_ChecksumType::None;
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

StatResult LocalDisk::TestDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    FileInfo file_info;
    StatResult ret = StatFile(filename.data, (int)StatFlag::SilentMissing, &file_info);

    if (ret == StatResult::Success && file_info.type != FileType::Directory) {
        LogError("Path '%1' is not a directory", filename);
        return StatResult::OtherError;
    }

    return ret;
}

Size LocalDisk::ReadFile(const char *path, Span<uint8_t> out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return K::ReadFile(filename.data, out_buf);
}

Size LocalDisk::ReadFile(const char *path, HeapArray<uint8_t> *out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return K::ReadFile(filename.data, Mebibytes(64), out_buf);
}

rk_WriteResult LocalDisk::WriteFile(const char *path, Span<const uint8_t> buf, const rk_WriteSettings &settings)
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
                return rk_WriteResult::OtherError;
            }
        }

        if (fd < 0) [[unlikely]] {
            LogError("Failed to create temporary file in '%1'", tmp);
            return rk_WriteResult::OtherError;
        }
    }

    K_DEFER_N(tmp_guard) {
        CloseDescriptor(fd);
        UnlinkFile(tmp.data);
    };

    StreamWriter writer(fd, filename.data);

    // Write encrypted content
    if (!writer.Write(buf))
        return rk_WriteResult::OtherError;
    if (!writer.Close())
        return rk_WriteResult::OtherError;

    // File is complete
    CloseDescriptor(fd);
    fd = -1;

    // Finalize!
    {
        unsigned int flags = settings.conditional ? 0 : (int)RenameFlag::Overwrite;
        RenameResult ret = RenameFile(tmp.data, filename.data, (int)RenameResult::AlreadyExists, flags);

        switch (ret) {
            case RenameResult::Success: {} break;
            case RenameResult::AlreadyExists: return rk_WriteResult::AlreadyExists;
            case RenameResult::OtherError: return rk_WriteResult::OtherError;
        }
    }

    tmp_guard.Disable();
    return rk_WriteResult::Success;
}

bool LocalDisk::DeleteFile(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1%/%2", url, path).len;

    return UnlinkFile(filename.data);
}

bool LocalDisk::RetainFile(const char *, int64_t)
{
    LogError("Cannot retain files with local backend");
    return false;
}

bool LocalDisk::ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func)
{
    BlockAllocator temp_alloc;

    path = path ? path : "";

    const char *dirname0 = path[0] ? Fmt(&temp_alloc, "%1/%2", url, path).ptr : url;
    Size prefix_len = strlen(url);

    if (!K::TestFile(dirname0, FileType::Directory))
        return true;

    HeapArray<const char *> pending_directories;
    pending_directories.Append(dirname0);

    for (Size i = 0; i < pending_directories.len; i++) {
        const char *dirname = pending_directories[i];

        EnumResult ret = EnumerateDirectory(dirname, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
            const char *filename = Fmt(&temp_alloc, "%1/%2", dirname, basename).ptr;
            const char *path = filename + prefix_len + 1;

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

StatResult LocalDisk::TestFile(const char *path, int64_t *out_size)
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

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<LocalDisk>(path);
    if (!disk->GetURL())
        return nullptr;
    return disk;
}

}
