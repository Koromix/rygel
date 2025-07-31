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

#pragma once

#include "src/core/base/base.hh"
#include "src/core/sqlite/sqlite.hh"

namespace RG {

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
