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
#include "config.hh"
#include "src/core/request/s3.hh"

namespace RG {

class S3Disk: public rk_Disk {
    s3_Session s3;

    int64_t retention;
    s3_LockMode lock;

public:
    S3Disk(const rk_S3Config &config);
    ~S3Disk() override;

    Size ReadFile(const char *path, Span<uint8_t> out_buf) override;
    Size ReadFile(const char *path, HeapArray<uint8_t> *out_buf) override;

    rk_WriteResult WriteFile(const char *path, Span<const uint8_t> buf, unsigned int flags) override;
    bool DeleteFile(const char *path) override;

    bool ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func) override;
    StatResult TestFile(const char *path, int64_t *out_size) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;
};

S3Disk::S3Disk(const rk_S3Config &config)
    : retention(config.retention), lock(config.lock)
{
    if (!s3.Open(config.remote))
        return;

    // We're good!
    url = s3.GetURL();
    default_threads = std::min(4 * GetCoreCount(), 64);
}

S3Disk::~S3Disk()
{
}

Size S3Disk::ReadFile(const char *path, Span<uint8_t> out_buf)
{
    return s3.GetObject(path, out_buf);
}

Size S3Disk::ReadFile(const char *path, HeapArray<uint8_t> *out_buf)
{
    return s3.GetObject(path, Mebibytes(256), out_buf);
}

rk_WriteResult S3Disk::WriteFile(const char *path, Span<const uint8_t> buf, unsigned int flags)
{
    s3_PutInfo info = { .conditional = !(flags & (int)rk_WriteFlag::Overwrite) };

    if (retention && (flags & (int)rk_WriteFlag::Lockable)) {
        info.retention = GetUnixTime() + retention;
        info.lock = lock;
    }

    s3_PutResult ret = s3.PutObject(path, buf, info);

    switch (ret) {
        case s3_PutResult::Success: return rk_WriteResult::Success;
        case s3_PutResult::ObjectExists: return rk_WriteResult::AlreadyExists;
        case s3_PutResult::OtherError: return rk_WriteResult::OtherError;
    }

    RG_UNREACHABLE();
}

bool S3Disk::DeleteFile(const char *path)
{
    return s3.DeleteObject(path);
}

bool S3Disk::ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func)
{
    char prefix[4096];

    if (path && !EndsWith(path, "/")) {
        Fmt(prefix, "%1/", path);
    } else {
        CopyString(path ? path : "", prefix);
    }

    return s3.ListObjects(prefix, func);
}

StatResult S3Disk::TestFile(const char *path, int64_t *out_size)
{
    return s3.HasObject(path, out_size);
}

bool S3Disk::CreateDirectory(const char *)
{
    // Directories don't really exist, it's just a prefix
    return true;
}

bool S3Disk::DeleteDirectory(const char *)
{
    // Directories don't really exist, it's just a prefix
    return true;
}

std::unique_ptr<rk_Disk> rk_OpenS3Disk(const rk_S3Config &config)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<S3Disk>(config);
    if (!disk->GetURL())
        return nullptr;
    return disk;
}

}
