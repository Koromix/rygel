// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

struct curl_slist;

namespace RG {

struct s3_Config {
    const char *scheme = nullptr;
    const char *host = nullptr;
    int port = -1;
    const char *region = nullptr; // Can be NULL
    const char *bucket = nullptr;
    bool path_mode = false;

    const char *access_id = nullptr;
    const char *access_key = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});

    bool Complete();
    bool Validate() const;

    void Clone(s3_Config *out_config) const;
};

bool s3_DecodeURL(Span<const char> url, s3_Config *out_config);

struct s3_PutInfo {
    const char *mimetype = nullptr;
    bool conditional = false;
};

enum class s3_PutResult {
    Success,
    ObjectExists,
    OtherError
};

class s3_Session {
    struct KeyValue {
        const char *key;
        const char *value;
    };

    s3_Config config;
    const char *url = nullptr;
    const char *region = nullptr;

    bool open = false;

    void *share = nullptr; // CURLSH
    std::mutex share_mutexes[8];

public:
    s3_Session();
    ~s3_Session();

    bool Open(const s3_Config &config);
    void Close();

    bool IsValid() const { return open; }
    const s3_Config &GetConfig() const { return config; }
    const char *GetURL() const { return url; }

    bool ListObjects(const char *prefix, FunctionRef<bool(const char *, int64_t)> func);
    Size GetObject(Span<const char> key, Span<uint8_t> out_buf);
    Size GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj);
    StatResult HasObject(Span<const char> key, int64_t *out_size = nullptr);
    s3_PutResult PutObject(Span<const char> key, Span<const uint8_t> data, const s3_PutInfo &info = {});
    bool DeleteObject(Span<const char> key);

private:
    bool OpenAccess();
    bool DetermineRegion(const char *url);

    void *InitConnection(); // CURL

    int RunSafe(const char *action, FunctionRef<int(void)> func, bool quick = false);

    Size PrepareHeaders(const char *method, const char *path, const char *query, Span<const KeyValue> kvs,
                        Span<const uint8_t> body, Allocator *alloc, Span<curl_slist> out_headers);
    void MakeSignature(const char *method, const char *path, const char *query,
                       const TimeSpec &date, const uint8_t sha256[32], uint8_t out_signature[32]);
    Span<char> MakeAuthorization(const uint8_t signature[32], const TimeSpec &date, Allocator *alloc);

    Span<const char> MakeURL(Span<const char> key, Allocator *alloc, Span<const char> *out_path = nullptr);
};

}
