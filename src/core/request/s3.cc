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

#include "src/core/base/base.hh"
#include "s3.hh"
#include "curl.hh"
#include "src/core/wrap/xml.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

// Fix mess caused by windows.h (included by libcurl)
#if defined(GetObject)
    #undef GetObject
#endif

static const char *GetS3Env(const char *name)
{
    K_ASSERT(strlen(name) < 64);

    static const char *const prefixes[] = {
        "S3_",
        "AWS_"
    };

    for (const char *prefix: prefixes) {
        char key[128];
        Fmt(key, "%1%2", prefix, name);

        const char *str = GetEnv(key);

        if (str)
            return str;
    }

    return nullptr;
}

bool s3_Config::SetProperty(Span<const char> key, Span<const char> value, Span<const char>)
{
    if (key == "Location" || key == "Endpoint") {
        return s3_DecodeURL(value, this);
    } else if (key == "Host") {
        host = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Port") {
        return ParseInt(value, &port);
    } else if (key == "Region") {
        region = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Bucket") {
        bucket = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Prefix") {
        prefix = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "AccessKeyID" || key == "KeyID") {
        access_id = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "SecretKey") {
        access_key = DuplicateString(value, &str_alloc).ptr;
        return true;
    }

    LogError("Unknown S3 property '%1'", key);
    return false;
}

bool s3_Config::Complete()
{
    if (!access_id) {
        const char *str = GetS3Env("ACCESS_KEY_ID");
        access_id = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    if (!access_key) {
        const char *str = GetS3Env("SECRET_ACCESS_KEY");

        if (str) {
            access_key = DuplicateString(str, &str_alloc).ptr;
        } else if (access_id && FileIsVt100(STDERR_FILENO)) {
            access_key = Prompt(T("AWS secret key:"), nullptr, "*", &str_alloc);
            if (!access_key)
                return false;
        }
    }

    return true;
}

bool s3_Config::Validate() const
{
    bool valid = true;

    if (!TestStr(scheme, "http") && !TestStr(scheme, "https")) {
        LogError("Invalid S3 scheme '%1'", scheme);
        valid = false;
    }
    if (!host) {
        LogError("Missing S3 host");
        valid = false;
    }
    if (!port || port > 65535) {
        LogError("Invalid S3 port");
        valid = false;
    }
    if (bucket && !bucket[0]) {
        LogError("Empty S3 bucket name");
        valid = false;
    }
    if (prefix && !prefix[0]) {
        LogError("Empty S3 prefix");
        valid = false;
    }

    if (!access_id) {
        LogError("Missing S3 access key ID (S3_ACCESS_KEY_ID) variable");
        return false;
    }
    if (!access_key) {
        LogError("Missing S3 secret key (S3_SECRET_ACCESS_KEY) variable");
        return false;
    }

    return valid;
}

void s3_Config::Clone(s3_Config *out_config) const
{
    out_config->str_alloc.Reset();

    out_config->scheme = scheme ? DuplicateString(scheme, &out_config->str_alloc).ptr : nullptr;
    out_config->host = host ? DuplicateString(host, &out_config->str_alloc).ptr : nullptr;
    out_config->port = port;
    out_config->region = region ? DuplicateString(region, &out_config->str_alloc).ptr : nullptr;
    out_config->bucket = bucket ? DuplicateString(bucket, &out_config->str_alloc).ptr : nullptr;
    out_config->prefix = prefix ? DuplicateString(prefix, &out_config->str_alloc).ptr : nullptr;
    out_config->access_id = access_id ? DuplicateString(access_id, &out_config->str_alloc).ptr : nullptr;
    out_config->access_key = access_key ? DuplicateString(access_key, &out_config->str_alloc).ptr : nullptr;
}

bool s3_DecodeURL(Span<const char> url, s3_Config *out_config)
{
    if (StartsWith(url, "s3:")) {
        url = url.Take(3, url.len - 3);
    }

    CURLU *h = curl_url();
    K_DEFER { curl_url_cleanup(h); };

    // Use CURLU to parse URL
    {
        char url0[32768];
        CopyString(url, url0);

        if (CURLUcode ret = curl_url_set(h, CURLUPART_URL, url0, 0); ret != CURLUE_OK) {
            LogError("Failed to parse URL '%1': %2", url, curl_url_strerror(ret));
            return false;
        }
    }

    const char *scheme = curl_GetUrlPartStr(h, CURLUPART_SCHEME, &out_config->str_alloc).ptr;
    Span<const char> host = curl_GetUrlPartStr(h, CURLUPART_HOST, &out_config->str_alloc);
    int port = curl_GetUrlPartInt(h, CURLUPART_PORT);

    const char *path = curl_GetUrlPartStr(h, CURLUPART_PATH, &out_config->str_alloc).ptr;
    K_ASSERT(path[0] == '/');

    const char *region = nullptr;
    bool virtual_mode = false;
    {
        Span<const char> remain = host;

        if (StartsWith(remain, "s3.")) {
            remain = remain.Take(3, remain.len - 3);
        } else {
            SplitStr(remain, ".s3.", &remain);
            virtual_mode = remain.len;
        }

        if (remain.len) {
            Size dots = (Size)std::count_if(remain.begin(), remain.end(), [](char c) { return c == '.'; });

            if (dots >= 2) {
                Span<const char> part = SplitStr(remain, '.');
                region = DuplicateString(part, &out_config->str_alloc).ptr;
            }
        }
    }

    const char *bucket = nullptr;
    const char *prefix = nullptr;

    if (virtual_mode) {
        prefix = path[1] ? path + 1 : nullptr;
    } else {
        bucket = path + 1;
        prefix = strchr(bucket, '/');

        if (prefix) {
            char *ptr = (char *)prefix++;
            ptr[0] = 0;

            prefix = prefix[0] ? prefix : nullptr;
        }
    }

    out_config->scheme = scheme;
    out_config->host = host.ptr;
    out_config->port = port;
    if (!out_config->region) {
        out_config->region = region;
    }
    out_config->bucket = bucket;
    out_config->prefix = prefix;

    return true;
}

static FmtArg FormatSha256(const uint8_t sha256[32])
{
    Span<const uint8_t> hash = MakeSpan(sha256, 32);
    return FmtHex(hash, FmtType::SmallHex);
}

static FmtArg FormatYYYYMMDD(const TimeSpec &date)
{
    FmtArg arg;

    arg.type = FmtType::Buffer;
    Fmt(arg.u.buf, "%1%2%3", FmtArg(date.year).Pad0(-4), FmtArg(date.month).Pad0(-2), FmtArg(date.day).Pad0(-2));

    return arg;
}

s3_Client::s3_Client()
{
    static_assert(CURL_LOCK_DATA_LAST <= K_LEN(share_mutexes));

    share = curl_share_init();

    curl_share_setopt(share, CURLSHOPT_LOCKFUNC, +[](CURL *, curl_lock_data data, curl_lock_access, void *udata) {
        s3_Client *s3 = (s3_Client *)udata;
        s3->share_mutexes[data].lock();
    });
    curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, +[](CURL *, curl_lock_data data, void *udata) {
        s3_Client *s3 = (s3_Client *)udata;
        s3->share_mutexes[data].unlock();
    });
    curl_share_setopt(share, CURLSHOPT_USERDATA, this);

    curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
}

s3_Client::~s3_Client()
{
    Close();

    curl_share_cleanup(share);
}

bool s3_Client::Open(const s3_Config &config)
{
    K_ASSERT(!open);

    if (!config.Validate())
        return false;

    config.Clone(&this->config);

    // Skip explicit port when not needed
    if (config.port == 80 && TestStr(config.scheme, "http")) {
        this->config.port = -1;
    } else if (config.port == 443 && TestStr(config.scheme, "https")) {
        this->config.port = -1;
    }

    if (!this->config.region) {
        const char *region1 = GetS3Env("REGION");
        const char *region2 = GetS3Env("DEFAULT_REGION");

        if (region1) {
            this->config.region = DuplicateString(region1, &this->config.str_alloc).ptr;
        } else if (region2) {
            this->config.region = DuplicateString(region2, &this->config.str_alloc).ptr;
        }
    }

    if (this->config.port > 0) {
        host = Fmt(&this->config.str_alloc, "%1:%2", config.host, this->config.port).ptr;
    } else {
        host = this->config.host;
    }
    url = Fmt(&this->config.str_alloc, "%1://%2%3%4%5%6", this->config.scheme, host,
              config.bucket ? "/" : "", config.bucket ? config.bucket : "",
              config.prefix ? "/" : "", config.prefix ? config.prefix : "").ptr;

    return OpenAccess();
}

void s3_Client::Close()
{
    for (CURL *curl: connections) {
        curl_easy_cleanup(curl);
    }
    connections.Clear();

    open = false;
    config = {};
}

bool s3_Client::ListObjects(Span<const char> prefix, FunctionRef<bool(const char *, int64_t)> func)
{
    BlockAllocator temp_alloc;

    Size skip_len = 0;
    char continuation[1024] = {};

    if (config.prefix) {
        skip_len = strlen(config.prefix) + 1;
        prefix = Fmt(&temp_alloc, "%1/%2", config.prefix, prefix).ptr;
    } else {
        // curl needs a C string
        prefix = DuplicateString(prefix, &temp_alloc);
    }

    KeyValue params[] = {
        { "continuation-token", continuation },
        { "list-type", "2" },
        { "prefix", prefix.ptr }
    };

    // Reuse for performance
    HeapArray<uint8_t> xml;

    for (;;) {
        int status = RunSafe("list S3 objects", 5, [&](CURL *curl) {
            int64_t now = GetUnixTime();
            TimeSpec date = DecomposeTimeUTC(now);

            PrepareRequest(curl, date, "GET", {}, params, &temp_alloc);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
                HeapArray<uint8_t> *xml = (HeapArray<uint8_t> *)udata;

                Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
                xml->Append(buf);

                return nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml);

            return curl_Perform(curl, nullptr);
        });
        if (status != 200)
            return false;

        pugi::xml_document doc;
        {
            pugi::xml_parse_result result = doc.load_buffer(xml.ptr, xml.len);

            if (!result) {
                LogError("Invalid XML returned by S3: %1", result.description());
                return false;
            }
        }

        pugi::xpath_node_set contents = doc.select_nodes("/ListBucketResult/Contents");
        bool truncated = doc.select_node("/ListBucketResult/IsTruncated").node().text().as_bool();

        for (const pugi::xpath_node &node: contents) {
            Span<const char> key = node.node().child("Key").text().get();
            int64_t size = node.node().child("Size").text().as_llong();

            if (key.len <= skip_len) [[unlikely]]
                continue;

            if (!func(key.ptr + skip_len, size))
                return false;
        }

        if (!truncated)
            break;

        xml.RemoveFrom(0);

        const char *token = doc.select_node("/ListBucketResult/NextContinuationToken").node().text().get();
        CopyString(token, continuation);
    }

    return true;
}

int64_t s3_Client::GetObject(Span<const char> key, FunctionRef<bool(int64_t, Span<const uint8_t>)> func, s3_ObjectInfo *out_info)
{
    BlockAllocator temp_alloc;

    if (config.prefix) {
        key = Fmt(&temp_alloc, "%1/%2", config.prefix, key);
    }

    struct GetContext {
        Span<const char> key;
        FunctionRef<bool(int64_t, Span<const uint8_t>)> func;
        int64_t offset;
    };

    GetContext ctx;

    ctx.key = key;
    ctx.func = func;
    ctx.offset = 0;

    int status = RunSafe("get S3 object", 5, 404, [&](CURL *curl) {
        int64_t now = GetUnixTime();
        TimeSpec date = DecomposeTimeUTC(now);

        PrepareRequest(curl, date, "GET", key, {}, &temp_alloc);

        // Handle restart
        ctx.offset = 0;

        if (out_info) {
            MemSet(out_info->version, 0, K_SIZE(out_info->version));

            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +[](char *buf, size_t, size_t nmemb, void *udata) {
                s3_ObjectInfo *out_info = (s3_ObjectInfo *)udata;

                Span<const char> value;
                Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
                value = TrimStr(value);

                if (TestStrI(key, "x-amz-version-id")) {
                    CopyString(value, out_info->version);
                }

                return nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, out_info);
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            GetContext *ctx = (GetContext *)udata;
            Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);

            if (!ctx->func(ctx->offset, buf))
                return (size_t)0;
            ctx->offset += buf.len;

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        return curl_Perform(curl, nullptr);
    });

    if (status != 200) {
        if (status == 404) {
            LogError("Cannot find S3 object '%1'", key);
        }
        return -1;
    }

    if (out_info) {
        out_info->size = ctx.offset;
    }
    return ctx.offset;
}

Size s3_Client::GetObject(Span<const char> key, Span<uint8_t> out_buf, s3_ObjectInfo *out_info)
{
    int64_t size = GetObject(key, [&](int64_t offset, Span<const uint8_t> buf) {
        Size copy_len = (Size)std::clamp(out_buf.len - offset, (int64_t)0, (int64_t)buf.len);
        MemCpy(out_buf.ptr + (Size)offset, buf.ptr, copy_len);

        return true;
    }, out_info);

    Size copied = (Size)std::min(size, (int64_t)out_buf.len);
    return (Size)copied;
}

Size s3_Client::GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj, s3_ObjectInfo *out_info)
{
    int64_t prev_len = out_obj->len;
    K_DEFER_N(err_guard) { out_obj->RemoveFrom(prev_len); };

    int64_t size = GetObject(key, [&](int64_t offset, Span<const uint8_t> buf) {
        out_obj->len = offset ? out_obj->len : prev_len;

        if (max_len >= 0 && out_obj->len - prev_len > max_len - buf.len) {
            LogError("S3 object '%1' is too big (max = %2)", key, FmtDiskSize(max_len));
            return false;
        }

        out_obj->Append(buf);
        return true;
    }, out_info);
    if (size < 0)
        return -1;

    err_guard.Disable();
    return out_obj->len - prev_len;
}

StatResult s3_Client::HeadObject(Span<const char> key, s3_ObjectInfo *out_info)
{
    BlockAllocator temp_alloc;

    if (config.prefix) {
        key = Fmt(&temp_alloc, "%1/%2", config.prefix, key);
    }

    int status = RunSafe("test S3 object", 5, 404, [&](CURL *curl) {
        int64_t now = GetUnixTime();
        TimeSpec date = DecomposeTimeUTC(now);

        PrepareRequest(curl, date, "HEAD", key, {}, &temp_alloc);

        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD

        if (out_info) {
            MemSet(out_info->version, 0, K_SIZE(out_info->version));

            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +[](char *buf, size_t, size_t nmemb, void *udata) {
                s3_ObjectInfo *out_info = (s3_ObjectInfo *)udata;

                Span<const char> value;
                Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
                value = TrimStr(value);

                if (TestStrI(key, "x-amz-version-id")) {
                    CopyString(value, out_info->version);
                }

                return nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, out_info);
        }

        int ret = curl_Perform(curl, nullptr);

        if (out_info && ret == 200) {
            curl_off_t length = 0;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
            out_info->size = (int64_t)length;
        }

        return ret;
    });

    switch (status) {
        case 200: return StatResult::Success;
        case 404: return StatResult::MissingPath;
        default: {
            LogError("Failed to stat object '%1': error %2", key, status);
            return StatResult::OtherError;
        } break;
    }
}

static inline const char *GetLockModeString(s3_LockMode mode)
{
    switch (mode) {
        case s3_LockMode::Governance: return "GOVERNANCE";
        case s3_LockMode::Compliance: return "COMPLIANCE";
    }

    K_UNREACHABLE();
}

s3_PutResult s3_Client::PutObject(Span<const char> key, int64_t size,
                                  FunctionRef<Size(int64_t offset, Span<uint8_t>)> func, const s3_PutSettings &settings)
{
    BlockAllocator temp_alloc;

    if (config.prefix) {
        key = Fmt(&temp_alloc, "%1/%2", config.prefix, key);
    }

    struct PutContext {
        FunctionRef<Size(int64_t offset, Span<uint8_t>)> func;
        int64_t offset;
    };

    PutContext ctx;

    ctx.func = func;
    ctx.offset = 0;

    int status = RunSafe("upload S3 object", 5, 412, [&](CURL *curl) {
        LocalArray<KeyValue, 16> headers;

        int64_t now = GetUnixTime();
        TimeSpec date = DecomposeTimeUTC(now);

        if (settings.mimetype) {
            headers.Append({ "Content-Type", settings.mimetype });
        }
        if (settings.conditional) {
            headers.Append({ "If-None-Match", "*" });
        }

        if (settings.checksum != s3_ChecksumType::None) {
            const char *header = nullptr;
            LocalArray<uint8_t, 32> hash;

            switch (settings.checksum) {
                case s3_ChecksumType::None: { K_UNREACHABLE(); } break;

                case s3_ChecksumType::CRC32: {
                    header = "x-amz-checksum-crc32";

                    uint32_t le = BigEndian(settings.hash.crc32);
                    MemCpy(hash.data, &le, 4);
                    hash.len = 4;
                } break;
                case s3_ChecksumType::CRC32C: {
                    header = "x-amz-checksum-crc32c";

                    uint32_t le = BigEndian(settings.hash.crc32c);
                    MemCpy(hash.data, &le, 4);
                    hash.len = 4;
                } break;
                case s3_ChecksumType::CRC64nvme: {
                    header = "x-amz-checksum-crc64nvme";

                    uint64_t le = BigEndian(settings.hash.crc64nvme);
                    MemCpy(hash.data, &le, 8);
                    hash.len = 8;
                } break;
                case s3_ChecksumType::SHA1: {
                    header = "x-amz-checksum-sha1";

                    MemCpy(hash.data, settings.hash.sha1, 20);
                    hash.len = 20;
                } break;
                case s3_ChecksumType::SHA256: {
                    header = "x-amz-checksum-sha256";

                    MemCpy(hash.data, settings.hash.sha256, 32);
                    hash.len = 32;
                } break;
            }

            Span<char> base64 = AllocateSpan<char>(&temp_alloc, 128);
            sodium_bin2base64(base64.ptr, base64.len, hash.data, hash.len, sodium_base64_VARIANT_ORIGINAL);

            headers.Append({ header, base64.ptr });
        }

        // PrepareRequest() does not try to mess with custom headers, to avoid sorting issues
        headers.Append({ "x-amz-content-sha256", "UNSIGNED-PAYLOAD" });
        headers.Append({ "x-amz-date", Fmt(&temp_alloc, "%1", FmtTimeISO(date)).ptr });

        if (settings.retain) {
            TimeSpec spec = DecomposeTimeUTC(settings.retain);
            const char *until = Fmt(&temp_alloc, "%1", FmtTimeISO(spec)).ptr;

            headers.Append({ "x-amz-object-lock-mode", GetLockModeString(settings.lock) });
            headers.Append({ "x-amz-object-lock-retain-until-date", until });
        }

        PrepareRequest(curl, date, "PUT", key, {}, headers, &temp_alloc);

        // Handle restart
        ctx.offset = 0;

        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // PUT
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
            PutContext *ctx = (PutContext *)udata;
            Span<uint8_t> buf = MakeSpan((uint8_t *)ptr, (Size)(size * nmemb));

            Size ret = ctx->func(ctx->offset, buf);
            if (ret < 0)
                return (size_t)CURL_READFUNC_ABORT;
            ctx->offset += ret;

            return (size_t)ret;
        });
        curl_easy_setopt(curl, CURLOPT_READDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);

        return curl_Perform(curl, nullptr);
    });

    switch (status) {
        case 200: return s3_PutResult::Success;
        case 412: return s3_PutResult::ObjectExists;
        default: return s3_PutResult::OtherError;
    }
}

s3_PutResult s3_Client::PutObject(Span<const char> key, Span<const uint8_t> data, const s3_PutSettings &settings)
{
    s3_PutResult ret = PutObject(key, data.len, [&](int64_t offset, Span<uint8_t> buf) {
        Size copy_len = std::min(buf.len, data.len - (Size)offset);
        MemCpy(buf.ptr, data.ptr + (Size)offset, copy_len);

        return copy_len;
    }, settings);

    return ret;
}

bool s3_Client::DeleteObject(Span<const char> key)
{
    BlockAllocator temp_alloc;

    if (config.prefix) {
        key = Fmt(&temp_alloc, "%1/%2", config.prefix, key);
    }

    int status = RunSafe("delete S3 object", 5, 204, [&](CURL *curl) {
        int64_t now = GetUnixTime();
        TimeSpec date = DecomposeTimeUTC(now);

        PrepareRequest(curl, date, "DELETE", key, {}, &temp_alloc);

        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); // DELETE (duh)

        return curl_Perform(curl, nullptr);
    });
    if (status != 200 && status != 204)
        return false;

    return true;
}

bool s3_Client::RetainObject(Span<const char> key, int64_t until, s3_LockMode mode)
{
    BlockAllocator temp_alloc;

    if (config.prefix) {
        key = Fmt(&temp_alloc, "%1/%2", config.prefix, key);
    }

    static const char *xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Retention xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <RetainUntilDate>%1</RetainUntilDate>
  <Mode>%2</Mode>
</Retention>
)";

    TimeSpec spec = DecomposeTimeUTC(until);
    Span<const char> body = Fmt(&temp_alloc, xml, FmtTimeISO(spec), GetLockModeString(mode));

    int status = RunSafe("retain S3 object", 5, [&](CURL *curl) {
        int64_t now = GetUnixTime();
        TimeSpec date = DecomposeTimeUTC(now);

        const KeyValue params[] = {{ "retention", nullptr }};
        PrepareRequest(curl, date, "PUT", key, params, &temp_alloc);

        Span<const char> remain = body;

        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // PUT
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
            Span<const char> *remain = (Span<const char> *)udata;
            Size give = std::min((Size)(size * nmemb), remain->len);

            MemCpy(ptr, remain->ptr, give);
            remain->ptr += (Size)give;
            remain->len -= (Size)give;

            return (size_t)give;
        });
        curl_easy_setopt(curl, CURLOPT_READDATA, &remain);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)remain.len);

        return curl_Perform(curl, nullptr);
    });
    if (status != 200)
        return false;

    return true;
}

bool s3_Client::OpenAccess()
{
    K_ASSERT(!open);

    BlockAllocator temp_alloc;

    region = config.region;

    // Try to guess region with anonymous GET request
    if (!region) {
        CURL *curl = ReserveConnection();
        if (!curl)
            return false;
        K_DEFER { ReleaseConnection(curl); };

        // Garage sends back the correct region in its XML Error message, maybe others do too
        HeapArray<uint8_t> xml;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +[](char *buf, size_t, size_t nmemb, void *udata) {
            s3_Client *session = (s3_Client *)udata;

            Span<const char> value;
            Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
            value = TrimStr(value);

            if (TestStrI(key, "x-amz-bucket-region")) {
                session->region = DuplicateString(value, &session->config.str_alloc).ptr;
            }

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<uint8_t> *xml = (HeapArray<uint8_t> *)udata;

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
            xml->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml);

        if (curl_Perform(curl, "S3") < 0)
            return false;

        if (!region && xml.len) {
            pugi::xml_document doc;

            if (doc.load_buffer(xml.ptr, xml.len)) {
                const char *str = doc.select_node("/Error/Region").node().text().get();
                region = str[0] ? DuplicateString(str, &config.str_alloc).ptr : nullptr;
            }
        }
    }

    if (!region) {
        // Many S3-compatible services don't really care, or accept us-east-1 for compatibility
        region = "us-east-1";
    }

    // Authentification test, adjust region if needed
    {
        int status = RunSafe("authenticate to S3 bucket", 3, 404, [&](CURL *curl) {
            int64_t now = GetUnixTime();
            TimeSpec date = DecomposeTimeUTC(now);

            PrepareRequest(curl, date, "HEAD", {}, {}, &temp_alloc);

            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +[](char *buf, size_t, size_t nmemb, void *udata) {
                s3_Client *session = (s3_Client *)udata;

                Span<const char> value;
                Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
                value = TrimStr(value);

                // Last chance to determine proper region
                if (!session->config.region && TestStrI(key, "x-amz-bucket-region")) {
                    session->region = DuplicateString(value, &session->config.str_alloc).ptr;
                }

                return nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

            int status = curl_Perform(curl, nullptr);

            if (status == 200 || status == 201)
                return 200;

            return status;
        });

        if (status == 404) {
            LogError("Unknown S3 bucket (error 404)");
            return false;
        } else if (status != 200) {
            return false;
        }
    }

    // Regenerate signing key in case the region has changed
    sign_day = 0;

    open = true;
    return true;
}

CURL *s3_Client::ReserveConnection()
{
    // Reuse existing connection
    {
        std::lock_guard<std::mutex> lock(connections_mutex);

        if (connections.len) {
            CURL *curl = connections.ptr[--connections.len];
            return curl;
        }
    }

    CURL *curl = curl_Init();
    if (!curl)
        return nullptr;
    curl_easy_setopt(curl, CURLOPT_SHARE, share);

    return curl;
}

void s3_Client::ReleaseConnection(CURL *curl)
{
    if (!curl)
        return;

    curl_Reset(curl);

    std::lock_guard<std::mutex> lock(connections_mutex);
    connections.Append(curl);
}

static inline bool ShouldRetry(int status)
{
    if (status == 409) // Transient conflict
        return true;

    if (status == 500) // Internal server error
        return true;
    if (status == 502) // Gateway error
        return true;
    if (status == 503) // Slow down
        return true;
    if (status == 504) // Gateway timeout
        return true;

    return false;
}

int s3_Client::RunSafe(const char *action, int tries, int expect, FunctionRef<int(CURL *)> func)
{
    CURL *curl = ReserveConnection();
    if (!curl)
        return false;
    K_DEFER { ReleaseConnection(curl); };

    LocalArray<char, 16384> log;
    int status = 0;

    const auto write = +[](char *ptr, size_t, size_t nmemb, void *udata) {
        LocalArray<char, 16384> *log = (LocalArray<char, 16384> *)udata;

        // Avoid stack overflow
        nmemb = std::min(nmemb, (size_t)log->Available());

        Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
        log->Append(buf);

        return nmemb;
    };

    for (int i = 0; i < tries; i++) {
        log.RemoveFrom(0);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &log);

        status = func(curl);

        if (status == 200 || status == expect)
            return status;
        if (status > 0 && !ShouldRetry(status))
            break;

        // Connection may be busted for some reason, start from scratch
        curl_easy_cleanup(curl);
        curl = nullptr;

        int delay = 200 + 100 * (1 << i);
        delay += !!i * GetRandomInt(0, delay / 2);

        WaitDelay(delay);

        curl = ReserveConnection();
        if (!curl)
            return false;
    }

    if (status < 0) {
        LogError("Failed to perform S3 call: %1", curl_easy_strerror((CURLcode)-status));
    } else if (log.len) {
        LogError("Failed to %1 with status %2: %3", action, status, FmtEscape(log.Take()));
    } else {
        LogError("Failed to %1 with status %2", action, status);
    }

    return -1;
}

void s3_Client::PrepareRequest(CURL *curl, const TimeSpec &date, const char *method, Span<const char> key,
                               Span<const KeyValue> params, Allocator *alloc)
{
    const KeyValue headers[] = {
        { "x-amz-content-sha256", "UNSIGNED-PAYLOAD" },
        { "x-amz-date", Fmt(alloc, "%1", FmtTimeISO(date)).ptr }
    };

    PrepareRequest(curl, date, method, key, params, headers, alloc);
}

void s3_Client::PrepareRequest(CURL *curl, const TimeSpec &date, const char *method, Span<const char> key,
                               Span<const KeyValue> params, Span<const KeyValue> headers, Allocator *alloc)
{
    Span<const char> url;
    Span<const char> path;
    {
        HeapArray<char> buf(alloc);

        Fmt(&buf, "%1://%2", config.scheme, host);

        Size path_offset = buf.len;

        const char *bucket = config.bucket ? config.bucket : "";
        Fmt(&buf, "/%1", FmtUrlSafe(bucket, "-._~"));

        if (key.len) {
            bool separate = (buf[buf.len - 1] != '/');
            Fmt(&buf, "%1%2", separate ? "/" : "", FmtUrlSafe(key, "-._~/"));
        }

        Size path_end = buf.len;

        if (params.len) {
            Fmt(&buf, "?%1%2%3", FmtUrlSafe(params[0].key, "-._~"),
                                 params[0].value ? "=" : "", FmtUrlSafe(params[0].value, "-._~"));
            for (Size i = 1; i < params.len; i++) {
                const KeyValue &param = params[i];
                Fmt(&buf, "&%1%2%3", FmtUrlSafe(param.key, "-._~"),
                                     param.value ? "=" : "", FmtUrlSafe(param.value, "-._~"));
            }
        }

        url = buf.Leak();
        path = MakeSpan(url.ptr + path_offset, path_end - path_offset);
    }

    Span<curl_slist> header;
    {
        HeapArray<curl_slist> list(alloc);

        // Avoid reallocation if possible
        list.Reserve(32);

        const char *authorization = MakeAuthorization(date, method, path, params, headers, alloc);
        list.Append({ (char *)authorization, nullptr });

        for (const KeyValue &header: headers) {
            const char *str = Fmt(alloc, "%1: %2", header.key, FmtUrlSafe(header.value, "-._~*$+/=")).ptr;
            list.Append({ (char *)str, nullptr });
        }

        for (Size i = 0; i < list.len - 1; i++) {
            list.ptr[i].next = list.ptr + i + 1;
        }
        list.ptr[list.len - 1].next = nullptr;

        header = list.Leak();
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header.ptr);
}

static void HmacSha256(Span<const uint8_t> key, Span<const char> message, uint8_t out_digest[32])
{
    static_assert(crypto_hash_sha256_BYTES == 32);

    uint8_t padded_key[64];

    // Hash and/or pad key
    if (key.len > K_SIZE(padded_key)) {
        crypto_hash_sha256(padded_key, key.ptr, (size_t)key.len);
        MemSet(padded_key + 32, 0, K_SIZE(padded_key) - 32);
    } else {
        MemCpy(padded_key, key.ptr, key.len);
        MemSet(padded_key + key.len, 0, K_SIZE(padded_key) - key.len);
    }

    // Inner hash
    uint8_t inner_hash[32];
    {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        for (Size i = 0; i < K_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36;
        }

        crypto_hash_sha256_update(&state, padded_key, K_SIZE(padded_key));
        crypto_hash_sha256_update(&state, (const uint8_t *)message.ptr, (size_t)message.len);
        crypto_hash_sha256_final(&state, inner_hash);
    }

    // Outer hash
    {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        for (Size i = 0; i < K_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36; // IPAD is still there
            padded_key[i] ^= 0x5C;
        }

        crypto_hash_sha256_update(&state, padded_key, K_SIZE(padded_key));
        crypto_hash_sha256_update(&state, inner_hash, K_SIZE(inner_hash));
        crypto_hash_sha256_final(&state, out_digest);
    }
}

const char *s3_Client::MakeAuthorization(const TimeSpec &date, const char *method, Span<const char> path,
                                         Span<const KeyValue> params, Span<const KeyValue> headers, Allocator *alloc)
{
    K_ASSERT(!date.offset);

    // Get signing key
    uint8_t key[32];
    {
        int day = (date.year << 16) | (date.month << 8) | date.day;
        std::shared_lock<std::shared_mutex> lock_shr(sign_mutex);

        if (day == sign_day) {
            MemCpy(key, sign_key, 32);
        } else {
            lock_shr.unlock();

            LocalArray<char, 256> secret;
            LocalArray<char, 256> ymd;
            secret.len = Fmt(secret.data, "AWS4%1", config.access_key).len;
            ymd.len = Fmt(ymd.data, "%1", FormatYYYYMMDD(date)).len;

            HmacSha256(secret.As<uint8_t>(), ymd, key);
            HmacSha256(key, region, key);
            HmacSha256(key, "s3", key);
            HmacSha256(key, "aws4_request", key);

            if (day > sign_day) {
                std::lock_guard<std::shared_mutex> lock_ex(sign_mutex);

                sign_day = day;
                MemCpy(sign_key, key, 32);
            }
        }
    }

    // Create canonical request
    Span<const char> canonical;
    {
        HeapArray<char> buf(alloc);

        Fmt(&buf, "%1\n%2\n", method, path);
        if (params.len) {
            Fmt(&buf, "%1=%2", FmtUrlSafe(params[0].key, "-._~"), FmtUrlSafe(params[0].value, "-._~"));
            for (Size i = 1; i < params.len; i++) {
                const KeyValue &param = params[i];
                Fmt(&buf, "&%1=%2", FmtUrlSafe(param.key, "-._~"), FmtUrlSafe(param.value, "-._~"));
            }
        }
        Fmt(&buf, "\nhost:%1\n", host);
        for (const KeyValue &header: headers) {
            Fmt(&buf, "%1:%2\n", FmtLowerAscii(header.key), FmtUrlSafe(header.value, "-._~*$+/="));
        }
        Fmt(&buf, "\nhost");
        for (const KeyValue &header: headers) {
            Fmt(&buf, ";%1", FmtLowerAscii(header.key));
        }
        Fmt(&buf, "\nUNSIGNED-PAYLOAD");

        canonical = buf.Leak();
    }

    // Create string to sign
    Span<const char> string;
    {
        HeapArray<char> buf(alloc);

        uint8_t hash[32];
        crypto_hash_sha256(hash, (const uint8_t *)canonical.ptr, (size_t)canonical.len);

        Fmt(&buf, "AWS4-HMAC-SHA256\n");
        Fmt(&buf, "%1\n", FmtTimeISO(date));
        Fmt(&buf, "%1/%2/s3/aws4_request\n", FormatYYYYMMDD(date), region);
        Fmt(&buf, "%1", FormatSha256(hash));

        string = buf.Leak();
    }

    // Create authorization header
    Span<char> authorization;
    {
        uint8_t signature[32];
        HmacSha256(key, string, signature);

        HeapArray<char> buf(alloc);

        Fmt(&buf, "Authorization: AWS4-HMAC-SHA256 ");
        Fmt(&buf, "Credential=%1/%2/%3/s3/aws4_request, ", config.access_id, FormatYYYYMMDD(date), region);
        Fmt(&buf, "SignedHeaders=host");
        for (const KeyValue &header: headers) {
            Fmt(&buf, ";%1", FmtLowerAscii(header.key));
        }
        Fmt(&buf, ", Signature=%1", FormatSha256(signature));

        authorization = buf.Leak();
    }

    return authorization.ptr;
}

}
