// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/sqlite/sqlite.hh"

namespace K {

struct rk_Config;
struct rk_S3Config;
struct ssh_Config;

enum class rk_ChecksumType {
    None,
    CRC32,
    CRC32C,
    CRC64nvme,
    SHA1,
    SHA256
};
static const char *const rk_ChecksumTypeNames[] = {
    "None",
    "CRC32",
    "CRC32C",
    "CRC64nvme",
    "SHA1",
    "SHA256"
};

struct rk_WriteSettings {
    bool conditional = false;
    int64_t retain = 0;

    rk_ChecksumType checksum = rk_ChecksumType::None;
    union {
        uint32_t crc32;
        uint32_t crc32c;
        uint64_t crc64nvme;
        uint8_t sha1[20];
        uint8_t sha256[32];
    } hash = {};
};

enum class rk_WriteResult {
    Success,
    AlreadyExists,
    OtherError
};

class rk_Disk {
protected:
    const char *url = nullptr;
    int default_threads = -1;

public:
    virtual ~rk_Disk();

    const char *GetURL() const { return url; }
    int GetDefaultThreads() const { return default_threads; }

    bool IsEmpty();

    virtual bool CreateDirectory(const char *path) = 0;
    virtual bool DeleteDirectory(const char *path) = 0;
    virtual StatResult TestDirectory(const char *path) = 0;

    virtual Size ReadFile(const char *path, Span<uint8_t> out_buf) = 0;
    virtual Size ReadFile(const char *path, HeapArray<uint8_t> *out_blob) = 0;

    // WriteResult::AlreadyExists must be silent, let the caller emit an error if relevant
    virtual rk_WriteResult WriteFile(const char *path, Span<const uint8_t> buf, const rk_WriteSettings &settings = {}) = 0;
    virtual bool DeleteFile(const char *path) = 0;
    virtual bool RetainFile(const char *path, int64_t retain) = 0;

    bool ListFiles(FunctionRef<bool(const char *, int64_t)> func) { return ListFiles(nullptr, func); }
    virtual bool ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func) = 0;
    virtual StatResult TestFile(const char *path, int64_t *out_size = nullptr) = 0;

    virtual rk_ChecksumType GetChecksumType() = 0;
};

std::unique_ptr<rk_Disk> rk_OpenDisk(const rk_Config &config);

std::unique_ptr<rk_Disk> rk_OpenLocalDisk(const char *path);
std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config);
std::unique_ptr<rk_Disk> rk_OpenS3Disk(const rk_S3Config &config);

}
