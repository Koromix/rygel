// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "s3.hh"
#include "curl.hh"
#include "http_misc.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/pugixml/src/pugixml.hpp"

namespace RG {

// Fix mess caused by windows.h (included by libcurl)
#ifdef GetObject
    #undef GetObject
#endif

bool s3_Config::SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory)
{
    if (key == "Location") {
        return s3_DecodeURL(value, this);
    } else if (key == "Host") {
        host = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Region") {
        region = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Bucket") {
        bucket = value[0] ? DuplicateString(value, &str_alloc).ptr : nullptr;
        return true;
    } else if (key == "PathMode") {
        return ParseBool(value, &path_mode);
    } else if (key == "AccessID") {
        access_id = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "AccessKey") {
        access_key = DuplicateString(value, &str_alloc).ptr;
        return true;
    }

    LogError("Unknown S3 property '%1'", key);
    return false;
}

bool s3_Config::Complete()
{
    if (!access_id) {
        const char *str = getenv("AWS_ACCESS_KEY_ID");
        access_id = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    if (!access_key) {
        const char *str = getenv("AWS_SECRET_ACCESS_KEY");

        if (str) {
            access_key = DuplicateString(str, &str_alloc).ptr;
        } else if (access_id && FileIsVt100(stderr)) {
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
    }
    if (!host) {
        LogError("Missing S3 host");
        valid = false;
    }
    if (!bucket) {
        LogError("Missing S3 bucket");
        valid = false;
    }

    if (!access_id) {
        LogError("Missing AWS key ID (AWS_ACCESS_KEY_ID) variable");
        return false;
    }

    if (!access_key) {
        LogError("Missing AWS secret key (AWS_SECRET_ACCESS_KEY) variable");
        return false;
    }

    return valid;
}

static Span<const char> GetUrlPart(CURLU *h, CURLUPart part, Allocator *alloc)
{
    char *buf = nullptr;

    CURLUcode ret = curl_url_get(h, part, &buf, 0);
    if (ret == CURLUE_OUT_OF_MEMORY)
        throw std::bad_alloc();
    RG_DEFER { curl_free(buf); };

    if (buf && buf[0]) {
        Span<const char> str = DuplicateString(buf, alloc);
        return str;
    } else {
        return {};
    }
}

bool s3_DecodeURL(Span<const char> url, s3_Config *out_config)
{
    CURLU *h = curl_url();
    RG_DEFER { curl_url_cleanup(h); };

    CURLUcode ret;
    {
        char url0[32768];

        url.len = std::min(url.len, RG_SIZE(url0) - 1);
        memcpy(url0, url.ptr, url.len);
        url0[url.len] = 0;

        ret = curl_url_set(h, CURLUPART_URL, url0, 0);
        if (ret != CURLUE_OK) {
            LogError("Failed to parse URL '%1': %2", url, curl_url_strerror(ret));
            return false;
        }
    }

    const char *scheme = GetUrlPart(h, CURLUPART_SCHEME, &out_config->str_alloc).ptr;
    Span<const char> host = GetUrlPart(h, CURLUPART_HOST, &out_config->str_alloc);
    const char *path = GetUrlPart(h, CURLUPART_PATH, &out_config->str_alloc).ptr;

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
            Span<const char> part = SplitStr(remain, '.', &remain);

            if (StartsWith(remain, "s3.")) {
                if (path_mode) {
                    LogError("Duplicate bucket name in S3 URL '%1'", url);
                    return false;
                }

                bucket = DuplicateString(part, &out_config->str_alloc).ptr;
            } else {
                region = DuplicateString(part, &out_config->str_alloc).ptr;
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
    out_config->region = region;
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

bool s3_Session::Open(const s3_Config &config)
{
    RG_ASSERT(!open);

    if (!config.Validate())
        return false;

    this->scheme = DuplicateString(config.scheme, &str_alloc).ptr;
    this->host = DuplicateString(config.host, &str_alloc).ptr;
    if (config.region) {
        this->region = DuplicateString(config.region, &str_alloc).ptr;
    } else {
        const char *region = getenv("AWS_REGION");
        this->region = region ? DuplicateString(region, &str_alloc).ptr : nullptr;
    }
    this->bucket = DuplicateString(config.bucket, &str_alloc).ptr;
    this->path_mode = config.path_mode;

    if (path_mode) {
        url = Fmt(&str_alloc, "%1://%2/%3", scheme, host, bucket).ptr;
    } else {
        url = Fmt(&str_alloc, "%1://%2/", scheme, host).ptr;
    }

    return OpenAccess(config.access_id, config.access_key);
}

void s3_Session::Close()
{
    open = false;

    scheme = nullptr;
    host = nullptr;
    url = nullptr;
    region = nullptr;
    bucket = nullptr;

    str_alloc.ReleaseAll();
}

bool s3_Session::ListObjects(const char *prefix, Allocator *alloc, HeapArray<const char *> *out_keys)
{
    BlockAllocator temp_alloc;

    RG_DEFER_NC(out_guard, len = out_keys->len) { out_keys->RemoveFrom(len); };

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    prefix = prefix ? prefix : "";

    Span<const char> path;
    Span<const char> url = MakeURL({}, &temp_alloc, &path);
    const char *after = "";

    // Reuse for performance
    HeapArray<char> query;
    HeapArray<uint8_t> xml;

    for (;;) {
        query.RemoveFrom(0);
        xml.RemoveFrom(0);

        Fmt(&query, "list-type=2&prefix="); http_EncodeUrlSafe(prefix, &query);
        Fmt(&query, "&start-after="); http_EncodeUrlSafe(after, &query);

        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("GET", path.ptr, query.ptr, {}, &temp_alloc, headers.data);

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

            if (!success) {
                LogError("Failed to set libcurl options");
                return false;
            }
        }

        int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
        if (status < 0)
            return false;
        if (status != 200) {
            LogError("Failed to list S3 objects with status %1", status);
            return false;
        }

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

            if (RG_LIKELY(key && key[0])) {
                key = DuplicateString(key, alloc).ptr;
                out_keys->Append(key);
            }
        }

        if (!truncated)
            break;
        RG_ASSERT(contents.size() > 0);

        after = out_keys->ptr[out_keys->len - 1];
    }

    out_guard.Disable();
    return true;
}

Size s3_Session::GetObject(Span<const char> key, Span<uint8_t> out_buf)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return -1;
    RG_DEFER { curl_easy_cleanup(curl); };

    Span<const char> path;
    Span<const char> url = MakeURL(key, &temp_alloc, &path);

    LocalArray<curl_slist, 32> headers;
    headers.len = PrepareHeaders("GET", path.ptr, nullptr, {}, &temp_alloc, headers.data);

    struct GetContext {
        Span<const char> key;
        Span<uint8_t> out;
        Size len;
    };
    GetContext ctx = {};
    ctx.key = key;
    ctx.out = out_buf;

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

        success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                     +[](char *ptr, size_t, size_t nmemb, void *udata) {
            GetContext *ctx = (GetContext *)udata;

            Size copy_len = std::min((Size)nmemb, ctx->out.len - ctx->len);
            memcpy_safe(ctx->out.ptr + ctx->len, ptr, (size_t)copy_len);
            ctx->len += copy_len;

            return nmemb;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        if (!success) {
            LogError("Failed to set libcurl options");
            return -1;
        }
    }

    int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
    if (status < 0)
        return -1;
    if (status != 200) {
        LogError("Failed to get S3 object with status %1", status);
        return -1;
    }

    return ctx.len;
}

Size s3_Session::GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj)
{
    BlockAllocator temp_alloc;

    Size prev_len = out_obj->len;
    RG_DEFER_N(out_guard) { out_obj->RemoveFrom(prev_len); };

    CURL *curl = curl_Init();
    if (!curl)
        return -1;
    RG_DEFER { curl_easy_cleanup(curl); };

    Span<const char> path;
    Span<const char> url = MakeURL(key, &temp_alloc, &path);

    LocalArray<curl_slist, 32> headers;
    headers.len = PrepareHeaders("GET", path.ptr, nullptr, {}, &temp_alloc, headers.data);

    struct GetContext {
        Span<const char> key;
        HeapArray<uint8_t> *out;
        Size max_len;
        Size total_len;
    };
    GetContext ctx = {};
    ctx.key = key;
    ctx.out = out_obj;
    ctx.max_len = max_len;

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

        success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                     +[](char *ptr, size_t, size_t nmemb, void *udata) {
            GetContext *ctx = (GetContext *)udata;

            if (ctx->max_len >= 0 && ctx->total_len > ctx->max_len - (Size)nmemb) {
                LogError("S3 object '%1' is too big (max = %2)", ctx->key, FmtDiskSize(ctx->max_len));
                return (size_t)0;
            }
            ctx->total_len += (Size)nmemb;

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
            ctx->out->Append(buf);

            return nmemb;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        if (!success) {
            LogError("Failed to set libcurl options");
            return -1;
        }
    }

    int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
    if (status < 0)
        return false;
    if (status != 200) {
        LogError("Failed to get S3 object with status %1", status);
        return false;
    }

    out_guard.Disable();
    return out_obj->len - prev_len;
}

bool s3_Session::HasObject(Span<const char> key)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    Span<const char> path;
    Span<const char> url = MakeURL(key, &temp_alloc, &path);

    LocalArray<curl_slist, 32> headers;
    headers.len = PrepareHeaders("HEAD", path.ptr, nullptr, {}, &temp_alloc, headers.data);

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);
        success &= !curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
    if (status < 0)
        return false;
    if (status != 200 && status != 404) {
        LogError("Failed to test S3 object with status %1", status);
        return false;
    }

    bool exists = (status == 200);
    return exists;
}

bool s3_Session::PutObject(Span<const char> key, Span<const uint8_t> data, const char *mimetype)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    Span<const char> path;
    Span<const char> url = MakeURL(key, &temp_alloc, &path);

    LocalArray<curl_slist, 32> headers;
    headers.len = PrepareHeaders("PUT", path.ptr, nullptr, data, &temp_alloc, headers.data);

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // PUT
        success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

        success &= !curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
            Span<const uint8_t> *remain = (Span<const uint8_t> *)udata;
            size_t give = std::min(size * nmemb, (size_t)remain->len);

            memcpy_safe(ptr, remain->ptr, give);
            remain->ptr += (Size)give;
            remain->len -= (Size)give;

            return give;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_READDATA, &data);
        success &= !curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.len);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
    if (status < 0)
        return false;
    if (status != 200) {
        LogError("Failed to upload S3 object with status %1", status);
        return false;
    }

    return true;
}

bool s3_Session::DeleteObject(Span<const char> key)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    Span<const char> path;
    Span<const char> url = MakeURL(key, &temp_alloc, &path);

    LocalArray<curl_slist, 32> headers;
    headers.len = PrepareHeaders("DELETE", path.ptr, nullptr, {}, &temp_alloc, headers.data);

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
    if (status < 0)
        return false;
    if (status != 204) {
        LogError("Failed to delete S3 object with status %1", status);
        return false;
    }

    return true;
}

bool s3_Session::OpenAccess(const char *id, const char *key)
{
    BlockAllocator temp_alloc;

    RG_ASSERT(!open);

    Span<const char> path;
    Span<const char> url = MakeURL({}, &temp_alloc, &path);

    // Determine region if needed
    if (!region && !DetermineRegion(url.ptr))
        return false;

    // Test access
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        RG_DEFER { curl_easy_cleanup(curl); };

        LocalArray<curl_slist, 32> headers;
        headers.len = PrepareHeaders("GET", path.ptr, nullptr, {}, &temp_alloc, headers.data);

        // Set CURL options
        {
            bool success = true;

            success &= !curl_easy_setopt(curl, CURLOPT_URL, url.ptr);
            success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.data);

            success &= !curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                         +[](char *buf, size_t, size_t nmemb, void *udata) {
                s3_Session *session = (s3_Session *)udata;

                Span<const char> value;
                Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
                value = TrimStr(value);

                if (!session->region && TestStrI(key, "x-amz-bucket-region")) {
                    session->region = DuplicateString(value, &session->str_alloc).ptr;
                }

                return nmemb;
            });
            success &= !curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

            if (!success) {
                LogError("Failed to set libcurl options");
                return false;
            }
        }

        int status = curl_Perform(curl, "S3", [](int i, int status) { return i < 5 && (status < 0 || status >= 500); });
        if (status < 0)
            return false;
        if (status != 200 && status != 201) {
            LogError("Failed to authenticate to S3 bucket with status %1", status);
            return false;
        }
    }

    open = true;
    return true;
}

bool s3_Session::DetermineRegion(const char *url)
{
    RG_ASSERT(!open);

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);

        success &= !curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                     +[](char *buf, size_t, size_t nmemb, void *udata) {
            s3_Session *session = (s3_Session *)udata;

            Span<const char> value;
            Span<const char> key = TrimStr(SplitStr(MakeSpan(buf, nmemb), ':', &value));
            value = TrimStr(value);

            if (!session->region && TestStrI(key, "x-amz-bucket-region")) {
                session->region = DuplicateString(value, &session->str_alloc).ptr;
            }

            return nmemb;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

        if (!success) {
            LogError("Failed to set libcurl options");
            return false;
        }
    }

    int status = curl_Perform(curl, "S3", [&](int i, int) { return i < 5 && !region; });
    if (status < 0)
        return false;

    if (!region) {
        LogError("Failed to retrieve bucket region, please define AWS_REGION");
        return false;
    }

    return true;
}

Size s3_Session::PrepareHeaders(const char *method, const char *path, const char *query,
                                Span<const uint8_t> body, Allocator *alloc, Span<curl_slist> out_headers)
{
    int64_t now = GetUnixTime();
    TimeSpec date = DecomposeTime(now, TimeMode::UTC);

    // Compute SHA-256 and signature
    uint8_t signature[32];
    uint8_t sha256[32];
    crypto_hash_sha256(sha256, body.ptr, (size_t)body.len);
    MakeSignature(method, path, query, date, sha256, signature);

    Size len = 0;

    // Prepare request headers
    out_headers[len++].data = MakeAuthorization(signature, date, alloc).ptr;
    out_headers[len++].data = Fmt(alloc, "x-amz-date: %1", FmtTimeISO(date)).ptr;
    out_headers[len++].data = Fmt(alloc, "x-amz-content-sha256: %1", FormatSha256(sha256)).ptr;

    // Link request headers
    for (Size i = 0; i < len - 1; i++) {
        out_headers.ptr[i].next = out_headers.ptr + i + 1;
    }
    out_headers[len - 1].next = nullptr;

    return len;
}

static void HmacSha256(Span<const uint8_t> key, Span<const uint8_t> message, uint8_t out_digest[32])
{
    RG_STATIC_ASSERT(crypto_hash_sha256_BYTES == 32);

    uint8_t padded_key[64];

    // Hash and/or pad key
    if (key.len > RG_SIZE(padded_key)) {
        crypto_hash_sha256(padded_key, key.ptr, (size_t)key.len);
        memset_safe(padded_key + 32, 0, RG_SIZE(padded_key) - 32);
    } else {
        memcpy_safe(padded_key, key.ptr, (size_t)key.len);
        memset_safe(padded_key + key.len, 0, (size_t)(RG_SIZE(padded_key) - key.len));
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

void s3_Session::MakeSignature(const char *method, const char *path, const char *query,
                               const TimeSpec &date, const uint8_t sha256[32], uint8_t out_signature[32])
{
    RG_ASSERT(!date.offset);

    // Create canonical request
    uint8_t canonical[32];
    {
        LocalArray<char, 4096> buf;

        buf.len += Fmt(buf.TakeAvailable(), "%1\n%2\n%3\n", method, path, query ? query : "").len;
        buf.len += Fmt(buf.TakeAvailable(), "host:%1\nx-amz-content-sha256:%2\nx-amz-date:%3\n\n", host, FormatSha256(sha256), FmtTimeISO(date)).len;
        buf.len += Fmt(buf.TakeAvailable(), "host;x-amz-content-sha256;x-amz-date\n").len;
        buf.len += Fmt(buf.TakeAvailable(), "%1", FormatSha256(sha256)).len;

        crypto_hash_sha256(canonical, (const uint8_t *)buf.data, (size_t)buf.len);
    }

    // Create string to sign
    LocalArray<char, 4096> string;
    {
        string.len += Fmt(string.TakeAvailable(), "AWS4-HMAC-SHA256\n").len;
        string.len += Fmt(string.TakeAvailable(), "%1\n", FmtTimeISO(date)).len;
        string.len += Fmt(string.TakeAvailable(), "%1/%2/s3/aws4_request\n", FormatYYYYMMDD(date), region).len;
        string.len += Fmt(string.TakeAvailable(), "%1", FormatSha256(canonical)).len;
    }

    // Create signature
    {
        Span<uint8_t> signature = MakeSpan(out_signature, 32);

        LocalArray<char, 256> secret;
        LocalArray<char, 256> ymd;
        secret.len = Fmt(secret.data, "AWS4%1", access_key).len;
        ymd.len = Fmt(ymd.data, "%1", FormatYYYYMMDD(date)).len;

        HmacSha256(secret.As<uint8_t>(), ymd, out_signature);
        HmacSha256(signature, region, out_signature);
        HmacSha256(signature, "s3", out_signature);
        HmacSha256(signature, "aws4_request", out_signature);
        HmacSha256(signature, string, out_signature);
    }
}

Span<char> s3_Session::MakeAuthorization(const uint8_t signature[32], const TimeSpec &date, Allocator *alloc)
{
    RG_ASSERT(!date.offset);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "Authorization: AWS4-HMAC-SHA256 ");
    Fmt(&buf, "Credential=%1/%2/%3/s3/aws4_request, ", access_id, FormatYYYYMMDD(date), region);
    Fmt(&buf, "SignedHeaders=host;x-amz-content-sha256;x-amz-date, ");
    Fmt(&buf, "Signature=%1", FormatSha256(signature));

    return buf.TrimAndLeak(1);
}

Span<const char> s3_Session::MakeURL(Span<const char> key, Allocator *alloc, Span<const char> *out_path)
{
    RG_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "%1://%2", scheme, host);
    Size path_offset = buf.len;

    if (path_mode) {
        buf.Append('/');
        http_EncodeUrlSafe(bucket, &buf);
    }
    if (key.len) {
        buf.Append('/');
        http_EncodeUrlSafe(key, "/", &buf);
    }
    if (buf.len == path_offset) {
        Fmt(&buf, "/");
    }

    if (out_path) {
        *out_path = MakeSpan(buf.ptr + path_offset, buf.len - path_offset);
    }
    return buf.TrimAndLeak(1);
}

}
