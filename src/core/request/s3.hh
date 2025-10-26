// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"

struct curl_slist;

namespace K {

struct s3_Config {
    const char *scheme = "https";
    const char *host = nullptr;
    int port = -1;
    const char *region = nullptr; // Can be NULL
    const char *bucket = nullptr;
    const char *prefix = nullptr;

    const char *access_id = nullptr;
    const char *access_key = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});

    bool Complete();
    bool Validate() const;

    void Clone(s3_Config *out_config) const;
};

bool s3_DecodeURL(Span<const char> url, s3_Config *out_config);
const char *s3_MakeURL(const s3_Config &config, Allocator *alloc);

enum class s3_LockMode {
    Governance,
    Compliance
};
static const char *const s3_LockModeNames[] = {
    "Governance",
    "Compliance"
};

enum class s3_ChecksumType {
    None,
    CRC32,
    CRC32C,
    CRC64nvme,
    SHA1,
    SHA256
};

struct s3_PutSettings {
    const char *mimetype = nullptr;
    bool conditional = false;

    int64_t retain = 0;
    s3_LockMode lock = s3_LockMode::Governance;

    s3_ChecksumType checksum = s3_ChecksumType::None;
    union {
        uint32_t crc32;
        uint32_t crc32c;
        uint64_t crc64nvme;
        uint8_t sha1[20];
        uint8_t sha256[32];
    } hash;
};

enum class s3_PutResult {
    Success,
    ObjectExists,
    OtherError
};

struct s3_ObjectInfo {
    int64_t size;
    char version[256];
};

class s3_Client {
    struct KeyValue {
        const char *key;
        const char *value;
    };

    s3_Config config;
    const char *host = nullptr;
    const char *url = nullptr;
    const char *region = nullptr;

    bool open = false;

    std::shared_mutex sign_mutex;
    int sign_day = 0;
    uint8_t sign_key[32];

    std::mutex connections_mutex;
    HeapArray<void *> connections; // CURL

    void *share = nullptr; // CURLSH
    std::mutex share_mutexes[8];

public:
    s3_Client();
    ~s3_Client();

    bool Open(const s3_Config &config);
    void Close();

    bool IsValid() const { return open; }
    const s3_Config &GetConfig() const { return config; }
    const char *GetURL() const { return url; }

    bool ListObjects(FunctionRef<bool(const char *, int64_t)> func) { return ListObjects(nullptr, func); }
    bool ListObjects(Span<const char> prefix, FunctionRef<bool(const char *, int64_t)> func);

    int64_t GetObject(Span<const char> key, FunctionRef<bool(int64_t, Span<const uint8_t>)> func, s3_ObjectInfo *out_info = nullptr);
    Size GetObject(Span<const char> key, Span<uint8_t> out_buf, s3_ObjectInfo *out_info = nullptr);
    Size GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj, s3_ObjectInfo *out_info = nullptr);
    StatResult HeadObject(Span<const char> key, s3_ObjectInfo *out_info = nullptr);

    s3_PutResult PutObject(Span<const char> key, int64_t size, FunctionRef<Size(int64_t, Span<uint8_t>)> func, const s3_PutSettings &settings = {});
    s3_PutResult PutObject(Span<const char> key, Span<const uint8_t> data, const s3_PutSettings &settings = {});
    bool DeleteObject(Span<const char> key);

    bool RetainObject(Span<const char> key, int64_t until, s3_LockMode mode);

private:
    bool OpenAccess();

    void *ReserveConnection(); // CURL
    void ReleaseConnection(void *curl); // CURL

    int RunSafe(const char *action, int tries, int expect, FunctionRef<int(void *, int)> func);
    int RunSafe(const char *action, int tries, FunctionRef<int(void *, int)> func) { return RunSafe(action, tries, 0, func); }

    void PrepareRequest(void *curl, const TimeSpec &date, const char *method, Span<const char> key,
                        Span<const KeyValue> params, Allocator *alloc);
    void PrepareRequest(void *curl, const TimeSpec &date, const char *method, Span<const char> key,
                        Span<const KeyValue> params, Span<const KeyValue> headers, Allocator *alloc);

    const char *MakeAuthorization(const TimeSpec &date, const char *method, Span<const char> path,
                                  Span<const KeyValue> params, Span<const KeyValue> kvs, Allocator *alloc);
};

}
