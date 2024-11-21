// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "src/core/request/s3.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

class S3Disk: public rk_Disk {
    s3_Session s3;

public:
    S3Disk(const s3_Config &config, const rk_OpenSettings &settings);
    ~S3Disk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_buf) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, FunctionRef<bool(const char *path)> func) override;
    StatResult TestRaw(const char *path) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;
};

S3Disk::S3Disk(const s3_Config &config, const rk_OpenSettings &settings)
{
    if (!s3.Open(config))
        return;

    // We're good!
    url = s3.GetURL();

    // S3 is slow unless you use parallelism
    threads = (settings.threads > 0) ? settings.threads : 100;
    compression_level = settings.compression_level;
}

S3Disk::~S3Disk()
{
}

bool S3Disk::Init(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

    return InitDefault(full_pwd, write_pwd);
}

Size S3Disk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    return s3.GetObject(path, out_buf);
}

Size S3Disk::ReadRaw(const char *path, HeapArray<uint8_t> *out_buf)
{
    return s3.GetObject(path, Mebibytes(256), out_buf);
}

Size S3Disk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    HeapArray<uint8_t> obj;
    if (!func([&](Span<const uint8_t> buf) { obj.Append(buf); return true; }))
        return -1;

    if (!s3.PutObject(path, obj))
        return -1;
    if (!PutCache(path))
        return -1;

    return obj.len;
}

bool S3Disk::DeleteRaw(const char *path)
{
    return s3.DeleteObject(path);
}

bool S3Disk::ListRaw(const char *path, FunctionRef<bool(const char *path)> func)
{
    char prefix[4096];

    if (path && !EndsWith(path, "/")) {
        Fmt(prefix, "%1/", path);
    } else {
        CopyString(path ? path : "", prefix);
    }

    return s3.ListObjects(prefix, func);
}

StatResult S3Disk::TestRaw(const char *path)
{
    return s3.HasObject(path);
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

std::unique_ptr<rk_Disk> rk_OpenS3Disk(const s3_Config &config, const char *username, const char *pwd, const rk_OpenSettings &settings)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<S3Disk>(config, settings);

    if (!disk->GetURL())
        return nullptr;
    if (username && !disk->Authenticate(username, pwd))
        return nullptr;

    return disk;
}

}
