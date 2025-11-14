// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "disk.hh"
#include "config.hh"
#include "lib/native/request/s3.hh"

namespace K {

class S3Disk: public rk_Disk {
    s3_Client s3;

    s3_LockMode lock;
    rk_ChecksumType checksum;

public:
    S3Disk(const rk_S3Config &config);
    ~S3Disk() override;

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
    StatResult TestFile(const char *path, int64_t *out_size) override;
};

S3Disk::S3Disk(const rk_S3Config &config)
    : lock(config.lock), checksum(config.checksum)
{
    if (!s3.Open(config.remote))
        return;

    // We're good!
    url = s3.GetURL();
    default_threads = std::min(8 * GetCoreCount(), 64);
}

S3Disk::~S3Disk()
{
}

rk_ChecksumType S3Disk::GetChecksumType()
{
    return checksum;
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

StatResult S3Disk::TestDirectory(const char *)
{
    // Directories don't really exist, it's just a prefix
    return StatResult::Success;
}

Size S3Disk::ReadFile(const char *path, Span<uint8_t> out_buf)
{
    return s3.GetObject(path, out_buf);
}

Size S3Disk::ReadFile(const char *path, HeapArray<uint8_t> *out_buf)
{
    return s3.GetObject(path, Mebibytes(256), out_buf);
}

rk_WriteResult S3Disk::WriteFile(const char *path, Span<const uint8_t> buf, const rk_WriteSettings &settings)
{
    s3_PutSettings put;

    put.conditional = settings.conditional;

    if (settings.retain) {
        put.retain = GetUnixTime() + settings.retain;
        put.lock = lock;
    }

    switch (settings.checksum) {
        case rk_ChecksumType::None: {} break;

        case rk_ChecksumType::CRC32: {
            put.checksum = s3_ChecksumType::CRC32;
            put.hash.crc32 = settings.hash.crc32;
        } break;
        case rk_ChecksumType::CRC32C: {
            put.checksum = s3_ChecksumType::CRC32C;
            put.hash.crc32c = settings.hash.crc32c;
        } break;
        case rk_ChecksumType::CRC64nvme: {
            put.checksum = s3_ChecksumType::CRC64nvme;
            put.hash.crc64nvme = settings.hash.crc64nvme;
        } break;
        case rk_ChecksumType::SHA1: {
            put.checksum = s3_ChecksumType::SHA1;
            MemCpy(put.hash.sha1, settings.hash.sha1, 20);
        } break;
        case rk_ChecksumType::SHA256: {
            put.checksum = s3_ChecksumType::SHA256;
            MemCpy(put.hash.sha256, settings.hash.sha256, 32);
        } break;
    }

    s3_PutResult ret = s3.PutObject(path, buf, put);

    switch (ret) {
        case s3_PutResult::Success: return rk_WriteResult::Success;
        case s3_PutResult::ObjectExists: return rk_WriteResult::AlreadyExists;
        case s3_PutResult::OtherError: return rk_WriteResult::OtherError;
    }

    K_UNREACHABLE();
}

bool S3Disk::DeleteFile(const char *path)
{
    return s3.DeleteObject(path);
}

bool S3Disk::RetainFile(const char *path, int64_t retain)
{
    int64_t until = GetUnixTime() + retain;
    return s3.RetainObject(path, until, lock);
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
    if (out_size) {
        s3_ObjectInfo info = {};
        StatResult ret = s3.HeadObject(path, &info);

        *out_size = info.size;
        return ret;
    } else {
        return s3.HeadObject(path);
    }
}

std::unique_ptr<rk_Disk> rk_OpenS3Disk(const rk_S3Config &config)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<S3Disk>(config);
    if (!disk->GetURL())
        return nullptr;
    return disk;
}

}
