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

namespace RG {

// Fix mess caused by windows.h (included by libcurl)
#if defined(GetObject)
    #undef GetObject
#endif

static const char *GetS3Env(const char *name)
{
    RG_ASSERT(strlen(name) < 64);

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
    if (key == "Location") {
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
        bucket = value.len ? DuplicateString(value, &str_alloc).ptr : nullptr;
        return true;
    } else if (key == "PathMode") {
        return ParseBool(value, &path_mode);
    } else if (key == "KeyID") {
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
            access_key = Prompt("AWS secret key: ", nullptr, "*", &str_alloc);
            if (!access_key)
                return false;
        }
    }

    return true;
}

bool s3_Config::Validate() const
{
    bool valid = true;

    if (!scheme) {
        LogError("Missing S3 protocol");
        valid = false;
    } else if (!TestStr(scheme, "http") && !TestStr(scheme, "https") && !TestStr(scheme, "s3")) {
        LogError("Invalid S3 protocol '%1'", scheme);
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
    if (!bucket) {
        LogError("Missing S3 bucket");
        valid = false;
    }

    if (!access_id) {
        LogError("Missing AWS key ID (S3_ACCESS_KEY_ID) variable");
        return false;
    }
    if (!access_key) {
        LogError("Missing AWS secret key (S3_SECRET_ACCESS_KEY) variable");
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
    out_config->path_mode = path_mode;
    out_config->access_id = access_id ? DuplicateString(access_id, &out_config->str_alloc).ptr : nullptr;
    out_config->access_key = access_key ? DuplicateString(access_key, &out_config->str_alloc).ptr : nullptr;
}

bool s3_DecodeURL(Span<const char> url, s3_Config *out_config)
{
    CURLU *h = curl_url();
    RG_DEFER { curl_url_cleanup(h); };

    CURLUcode ret;
    {
        char url0[32768];

        url.len = std::min(url.len, RG_SIZE(url0) - 1);
        MemCpy(url0, url.ptr, url.len);
        url0[url.len] = 0;

        ret = curl_url_set(h, CURLUPART_URL, url0, CURLU_NON_SUPPORT_SCHEME);
        if (ret != CURLUE_OK) {
            LogError("Failed to parse URL '%1': %2", url, curl_url_strerror(ret));
            return false;
        }
    }

    const char *scheme = curl_GetUrlPartStr(h, CURLUPART_SCHEME, &out_config->str_alloc).ptr;
    Span<const char> host = curl_GetUrlPartStr(h, CURLUPART_HOST, &out_config->str_alloc);
    int port = curl_GetUrlPartInt(h, CURLUPART_PORT);
    const char *path = curl_GetUrlPartStr(h, CURLUPART_PATH, &out_config->str_alloc).ptr;

    const char *bucket = nullptr;
    bool path_mode = false;
    const char *region = nullptr;

    // Extract bucket name from path (if any)
    if (path && !TestStr(path, "/")) {
        Span<const char> remain;
        Span<const char> name = SplitStr(path + 1, '/', &remain);

        if (remain.len) {
            LogError("Too many parts in S3 URL '%1'", url);
            return false;
        }

        bucket = DuplicateString(name, &out_config->str_alloc).ptr;
        path_mode = true;
    }

    // Extract bucket and region from host name
    {
        Span<const char> remain = host;

        if (!StartsWith(remain, "s3.")) {
            Span<const char> part = SplitStr(remain, ".s3.", &remain);

            if (remain.len) {
                if (path_mode) {
                    LogError("Duplicate bucket name in S3 URL '%1'", url);
                    return false;
                }

                bucket = DuplicateString(part, &out_config->str_alloc).ptr;

                remain.ptr -= 3;
                remain.len += 3;
            }
        }

        if (!region) {
            if (StartsWith(remain, "s3.")) {
                SplitStr(remain, '.', &remain);
            }

            Size dots = (Size)std::count_if(remain.begin(), remain.end(), [](char c) { return c == '.'; });

            if (dots >= 2) {
                Span<const char> part = SplitStr(remain, '.');
                region = DuplicateString(part, &out_config->str_alloc).ptr;
            }
        }
    }

    out_config->scheme = scheme;
    out_config->host = host.ptr;
    out_config->port = port;
    if (!out_config->region) {
        out_config->region = region;
    }
    out_config->bucket = bucket;
    out_config->path_mode = path_mode;

    return true;
}

static FmtArg FormatSha256(const uint8_t sha256[32])
{
    Span<const uint8_t> hash = MakeSpan(sha256, 32);
    return FmtSpan(hash, FmtType::SmallHex, "").Pad0(-2);
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
    static_assert(CURL_LOCK_DATA_LAST <= RG_LEN(share_mutexes));

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
    RG_ASSERT(!open);

    if (!config.Validate())
        return false;

    config.Clone(&this->config);

    // Skip explicit port when not needed
    if (TestStr(config.scheme, "s3")) {
        this->config.scheme = "https";
    }
    if (config.port == 80 && TestStr(config.scheme, "http")) {
        this->config.port = -1;
    } else if (config.port == 443 && TestStr(config.scheme, "https")) {
        this->config.port = -1;
    }

    if (!this->config.region) {
        const char *region = GetS3Env("REGION");
        this->config.region = region ? DuplicateString(region, &this->config.str_alloc).ptr : nullptr;
    }

    if (this->config.port > 0) {
        host = Fmt(&this->config.str_alloc, "%1:%2", config.host, this->config.port).ptr;
    } else {
        host = config.host;
    }
    url = Fmt(&this->config.str_alloc, "%1://%2/%3", this->config.scheme, host, config.path_mode ? config.bucket : "").ptr;

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

    const char *path;
    Span<const char> url = MakeURL({}, nullptr, &temp_alloc, &path);

    // Reuse for performance
    HeapArray<char> query;
    HeapArray<uint8_t> xml;

    Fmt(&query, "list-type=2&prefix=%1", FmtUrlSafe(prefix));

    for (;;) {
        int status = RunSafe("list S3 objects", [&](CURL *curl) {
            LocalArray<curl_slist, 32> headers;
            headers.len = PrepareHeaders("GET", path, query.ptr, {}, &temp_alloc, headers.data);

            // Set CURL options
            {
                bool success = true;

                success &= !curl_easy_setopt(curl, CURLOPT_URL, Fmt(&temp_alloc, "%1?%2", url, query).ptr);
                success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

                success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                             +[](char *ptr, size_t, size_t nmemb, void *udata) {
                    HeapArray<uint8_t> *xml = (HeapArray<uint8_t> *)udata;

                    Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
                    xml->Append(buf);

                    return nmemb;
                });
                success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml);

                if (!success)
                    return -CURLE_FAILED_INIT;
            }

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
            const char *key = node.node().child("Key").text().get();
            int64_t size = node.node().child("Size").text().as_llong();

            if (key && key[0]) [[likely]] {
                if (!func(key, size))
                    return false;
            }
        }

        if (!truncated)
            break;

        xml.RemoveFrom(0);
        query.RemoveFrom(0);

        const char *continuation = doc.select_node("/ListBucketResult/NextContinuationToken").node().text().get();
        Fmt(&query, "continuation-token=%1&list-type=2&prefix=%2", FmtUrlSafe(continuation), FmtUrlSafe(prefix));
    }

    return true;
}

int64_t s3_Client::GetObject(Span<const char> key, FunctionRef<bool(Span<const uint8_t>)> func)
{
    BlockAllocator temp_alloc;

    const char *path;
    Span<const char> url = MakeURL(key, nullptr, &temp_alloc, &path);

    struct GetContext {
        Span<const char> key;
        FunctionRef<bool(Span<const uint8_t>)> func;
        int64_t len;
    };

    GetContext ctx = {};
    ctx.key = key;
    ctx.func = func;

    int status = RunSafe("get S3 object", [&](CURL *curl) {
        ctx.len = 0;

        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("GET", path, nullptr, {}, &temp_alloc, headers.data);

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

            success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                         +[](char *ptr, size_t, size_t nmemb, void *udata) {
                GetContext *ctx = (GetContext *)udata;
                Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);

                if (!ctx->func(buf))
                    return (size_t)0;
                ctx->len += buf.len;

                return nmemb;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

            if (!success)
                return -CURLE_FAILED_INIT;
        }

        return curl_Perform(curl, nullptr);
    });

    if (status != 200) {
        if (status == 404) {
            LogError("Cannot find S3 object '%1'", key);
        }
        return -1;
    }

    return ctx.len;
}

Size s3_Client::GetObject(Span<const char> key, Span<uint8_t> out_buf)
{
    Size prev_len = out_buf.len;

    int64_t size = GetObject(key, [&](Span<const uint8_t> buf) {
        buf.len = std::min(buf.len, out_buf.len);
        MemCpy(out_buf.ptr, buf.ptr, buf.len);

        out_buf.ptr += buf.len;
        out_buf.len -= buf.len;

        return true;
    });
    if (size < 0)
        return -1;

    return prev_len - out_buf.len;
}

Size s3_Client::GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj)
{
    Size prev_len = out_obj->len;
    RG_DEFER_N(err_guard) { out_obj->RemoveFrom(prev_len); };

    int64_t size = GetObject(key, [&](Span<const uint8_t> buf) {
        if (max_len >= 0 && out_obj->len - prev_len > max_len - buf.len) {
            LogError("S3 object '%1' is too big (max = %2)", key, FmtDiskSize(max_len));
            return false;
        }

        out_obj->Append(buf);
        return true;
    });
    if (size < 0)
        return -1;

    err_guard.Disable();
    return out_obj->len - prev_len;
}

StatResult s3_Client::HasObject(Span<const char> key, int64_t *out_size)
{
    BlockAllocator temp_alloc;

    const char *path;
    Span<const char> url = MakeURL(key, nullptr, &temp_alloc, &path);

    int status = RunSafe("test S3 object", [&](CURL *curl) {
        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("HEAD", path, nullptr, {}, &temp_alloc, headers.data);

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);
            success &= !curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

            if (!success)
                return -CURLE_FAILED_INIT;
        }

        int ret = curl_Perform(curl, nullptr);

        if (out_size && ret == 200) {
            curl_off_t length = 0;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
            *out_size = (int64_t)length;
        }

        return ret;
    });

    switch (status) {
        case 200: return StatResult::Success;
        case 404: return StatResult::MissingPath;
        case 403: {
            LogError("Failed to stat object '%1': permission denied", key);
            return StatResult::AccessDenied;
        } break;
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

    RG_UNREACHABLE();
}

s3_PutResult s3_Client::PutObject(Span<const char> key, int64_t size, FunctionRef<Size(Span<uint8_t>)> func, const s3_PutSettings &settings)
{
    BlockAllocator temp_alloc;

    const char *path;
    Span<const char> url = MakeURL(key, nullptr, &temp_alloc, &path);

    LocalArray<KeyValue, 8> kvs;

    if (settings.mimetype) {
        kvs.Append({ "Content-Type", settings.mimetype });
    }
    if (settings.conditional) {
        kvs.Append({ "If-None-Match", "*" });
    }
    if (settings.retention) {
        TimeSpec spec = DecomposeTimeUTC(settings.retention);
        const char *until = Fmt(&temp_alloc, "%1", FmtTimeISO(spec)).ptr;

        kvs.Append({ "x-amz-object-lock-mode", GetLockModeString(settings.lock) });
        kvs.Append({ "x-amz-object-lock-retain-until-date", until });
    }

    int status = RunSafe("upload S3 object", [&](CURL *curl) {
        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("PUT", path, nullptr, kvs, &temp_alloc, headers.data);

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // PUT
            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

            success &= !curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
                FunctionRef<Size(Span<uint8_t>)> *func = (FunctionRef<Size(Span<uint8_t>)> *)udata;
                Span<uint8_t> buf = MakeSpan((uint8_t *)ptr, (Size)(size * nmemb));

                Size ret = (*func)(buf);
                if (ret < 0)
                    return (size_t)CURL_READFUNC_ABORT;

                return (size_t)ret;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_READDATA, &func);
            success &= !curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);

            if (!success)
                return -CURLE_FAILED_INIT;
        }

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
    s3_PutResult ret = PutObject(key, data.len, [&](Span<uint8_t> buf) {
        Size give = std::min(buf.len, data.len);
        MemCpy(buf.ptr, data.ptr, give);

        data.ptr += give;
        data.len -= give;

        return give;
    }, settings);

    return ret;
}

bool s3_Client::DeleteObject(Span<const char> key)
{
    BlockAllocator temp_alloc;

    const char *path;
    Span<const char> url = MakeURL(key, nullptr, &temp_alloc, &path);

    int status = RunSafe("delete S3 object", [&](CURL *curl) {
        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("DELETE", path, nullptr, {}, &temp_alloc, headers.data);

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

            if (!success)
                return -CURLE_FAILED_INIT;
        }

        int status = curl_Perform(curl, nullptr);

        if (status == 200 || status == 204)
            return 200;

        return status;
    });
    if (status != 200)
        return false;

    return true;
}

bool s3_Client::RetainObject(Span<const char> key, int64_t until, s3_LockMode mode)
{
    BlockAllocator temp_alloc;

    const char *path;
    Span<const char> url = MakeURL(key, "retention", &temp_alloc, &path);

    static const char *xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Retention xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <RetainUntilDate>%1</RetainUntilDate>
  <Mode>%2</Mode>
</Retention>
)";

    TimeSpec spec = DecomposeTimeUTC(until);
    Span<const char> body = Fmt(&temp_alloc, xml, FmtTimeISO(spec), GetLockModeString(mode));

    int status = RunSafe("retain S3 object", [&](CURL *curl) {
        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("PUT", path, "retention=", {}, &temp_alloc, headers.data);

        Span<const char> remain = body;

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // PUT
            success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

            success &= !curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
                Span<const char> *remain = (Span<const char> *)udata;
                Size give = std::min((Size)(size * nmemb), remain->len);

                MemCpy(ptr, remain->ptr, give);
                remain->ptr += (Size)give;
                remain->len -= (Size)give;

                return (size_t)give;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_READDATA, &remain);
            success &= !curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)remain.len);

            if (!success)
                return -CURLE_FAILED_INIT;
        }

        return curl_Perform(curl, nullptr);
    });
    if (status != 200)
        return false;

    return true;
}

bool s3_Client::OpenAccess()
{
    RG_ASSERT(!open);

    BlockAllocator temp_alloc;

    region = config.region;

    // Guess region with HEAD request
    if (!region) {
        CURL *curl = ReserveConnection();
        if (!curl)
            return false;
        RG_DEFER { ReleaseConnection(curl); };

        Span<const char> url = MakeURL({}, nullptr, &temp_alloc);

        // Garage sends back the correct region in its XML Error message, maybe others do too
        HeapArray<uint8_t> xml;

        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);

            success &= !curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                         +[](char *buf, size_t, size_t nmemb, void *udata) {
                s3_Client *session = (s3_Client *)udata;

                Span<const char> value;
                Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
                value = TrimStr(value);

                if (TestStrI(key, "x-amz-bucket-region")) {
                    session->region = DuplicateString(value, &session->config.str_alloc).ptr;
                }

                return nmemb;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

            success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                         +[](char *ptr, size_t, size_t nmemb, void *udata) {
                HeapArray<uint8_t> *xml = (HeapArray<uint8_t> *)udata;

                Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
                xml->Append(buf);

                return nmemb;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml);

            if (!success) {
                LogError("Failed to set libcurl options");
                return false;
            }
        }

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

    // Final authentification test
    {
        const char *path;
        Span<const char> url = MakeURL({}, nullptr, &temp_alloc, &path);

        int status = RunSafe("authenticate to S3 bucket", [&](CURL *curl) {
            LocalArray<curl_slist, 32> headers;
            headers.len = PrepareHeaders("HEAD", path, nullptr, {}, &temp_alloc, headers.data);

            // Set CURL options
            {
                bool success = true;

                success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
                success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);
                success &= !curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

                success &= !curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                             +[](char *buf, size_t, size_t nmemb, void *udata) {
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
                success &= !curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

                if (!success)
                    return -CURLE_FAILED_INIT;
            }

            int status = curl_Perform(curl, nullptr);

            if (status == 200 || status == 201)
                return 200;

            return status;
        }, true);

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

int s3_Client::RunSafe(const char *action, FunctionRef<int(CURL *)> func, bool quick)
{
    CURL *curl = ReserveConnection();
    if (!curl)
        return false;
    RG_DEFER { ReleaseConnection(curl); };

    int tries = quick ? 3 : 9;
    int status = 0;

    for (int i = 0; i < tries; i++) {
        status = func(curl);

        if (status == 200 || status == 404 || status == 412)
            return status;

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
    } else {
        LogError("Failed to %1 with status %2", action, status);
    }

    return -1;
}

Size s3_Client::PrepareHeaders(const char *method, const char *path, const char *query, Span<const KeyValue> kvs,
                               Allocator *alloc, Span<curl_slist> out_headers)
{
    int64_t now = GetUnixTime();
    TimeSpec date = DecomposeTimeUTC(now);

    uint8_t signature[32];
    MakeSignature(method, path, query, date, signature);

    Size len = 0;

    out_headers[len++].data = MakeAuthorization(signature, date, alloc).ptr;
    out_headers[len++].data = Fmt(alloc, "x-amz-date: %1", FmtTimeISO(date)).ptr;
    out_headers[len++].data = Fmt(alloc, "x-amz-content-sha256: UNSIGNED-PAYLOAD").ptr;

    for (const KeyValue &kv: kvs) {
        out_headers[len++].data = Fmt(alloc, "%1: %2", kv.key, kv.value).ptr;
    }

    // Link request headers
    for (Size i = 0; i < len - 1; i++) {
        out_headers.ptr[i].next = out_headers.ptr + i + 1;
    }
    out_headers[len - 1].next = nullptr;

    return len;
}

static void HmacSha256(Span<const uint8_t> key, Span<const uint8_t> message, uint8_t out_digest[32])
{
    static_assert(crypto_hash_sha256_BYTES == 32);

    uint8_t padded_key[64];

    // Hash and/or pad key
    if (key.len > RG_SIZE(padded_key)) {
        crypto_hash_sha256(padded_key, key.ptr, (size_t)key.len);
        MemSet(padded_key + 32, 0, RG_SIZE(padded_key) - 32);
    } else {
        MemCpy(padded_key, key.ptr, key.len);
        MemSet(padded_key + key.len, 0, RG_SIZE(padded_key) - key.len);
    }

    // Inner hash
    uint8_t inner_hash[32];
    {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        for (Size i = 0; i < RG_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36;
        }

        crypto_hash_sha256_update(&state, padded_key, RG_SIZE(padded_key));
        crypto_hash_sha256_update(&state, message.ptr, (size_t)message.len);
        crypto_hash_sha256_final(&state, inner_hash);
    }

    // Outer hash
    {
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        for (Size i = 0; i < RG_SIZE(padded_key); i++) {
            padded_key[i] ^= 0x36; // IPAD is still there
            padded_key[i] ^= 0x5C;
        }

        crypto_hash_sha256_update(&state, padded_key, RG_SIZE(padded_key));
        crypto_hash_sha256_update(&state, inner_hash, RG_SIZE(inner_hash));
        crypto_hash_sha256_final(&state, out_digest);
    }
}

static void HmacSha256(Span<const uint8_t> key, Span<const char> message, uint8_t out_digest[32])
{
    return HmacSha256(key, message.As<const uint8_t>(), out_digest);
}

void s3_Client::MakeSignature(const char *method, const char *path, const char *query, const TimeSpec &date, uint8_t out_signature[32])
{
    RG_ASSERT(!date.offset);

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
    LocalArray<char, 4096> canonical;
    canonical.len += Fmt(canonical.TakeAvailable(), "%1\n%2\n%3\n", method, path, query ? query : "").len;
    canonical.len += Fmt(canonical.TakeAvailable(), "host:%1\nx-amz-content-sha256:UNSIGNED-PAYLOAD\nx-amz-date:%2\n\n", host, FmtTimeISO(date)).len;
    canonical.len += Fmt(canonical.TakeAvailable(), "host;x-amz-content-sha256;x-amz-date\n").len;
    canonical.len += Fmt(canonical.TakeAvailable(), "UNSIGNED-PAYLOAD").len;

    // Hash canonical request
    uint8_t hash[32];
    crypto_hash_sha256(hash, (const uint8_t *)canonical.data, (size_t)canonical.len);

    // Create string to sign
    LocalArray<char, 4096> string;
    string.len += Fmt(string.TakeAvailable(), "AWS4-HMAC-SHA256\n").len;
    string.len += Fmt(string.TakeAvailable(), "%1\n", FmtTimeISO(date)).len;
    string.len += Fmt(string.TakeAvailable(), "%1/%2/s3/aws4_request\n", FormatYYYYMMDD(date), region).len;
    string.len += Fmt(string.TakeAvailable(), "%1", FormatSha256(hash)).len;

    // We've got everything we need!
    HmacSha256(key, string, out_signature);
}

Span<char> s3_Client::MakeAuthorization(const uint8_t signature[32], const TimeSpec &date, Allocator *alloc)
{
    RG_ASSERT(!date.offset);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "Authorization: AWS4-HMAC-SHA256 ");
    Fmt(&buf, "Credential=%1/%2/%3/s3/aws4_request, ", config.access_id, FormatYYYYMMDD(date), region);
    Fmt(&buf, "SignedHeaders=host;x-amz-content-sha256;x-amz-date, ");
    Fmt(&buf, "Signature=%1", FormatSha256(signature));

    return buf.TrimAndLeak(1);
}

Span<const char> s3_Client::MakeURL(Span<const char> key, const char *query, Allocator *alloc, const char **out_path)
{
    RG_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "%1://%2", config.scheme, host);

    Size path_offset = buf.len;

    if (config.path_mode) {
        Fmt(&buf, "/%1", FmtUrlSafe(config.bucket));
    }
    Fmt(&buf, "/%1", FmtUrlSafe(key, "/"));

    Size path_end = buf.len;

    if (query) {
        Fmt(&buf, "?%1", query);
    }

    if (out_path) {
        Span<const char> path = MakeSpan(buf.ptr + path_offset, path_end - path_offset);
        *out_path = (path_end < buf.len) ? DuplicateString(path, alloc).ptr : path.ptr;
    }

    return buf.TrimAndLeak(1);
}

}
