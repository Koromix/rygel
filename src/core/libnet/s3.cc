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
        return "";
    }
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

static FmtArg FormatRfcDate(const TimeSpec &date)
{
    FmtArg arg;

    const char *week_day = nullptr;
    switch (date.week_day) {
        case 1: { week_day = "Mon"; } break;
        case 2: { week_day = "Tue"; } break;
        case 3: { week_day = "Wed"; } break;
        case 4: { week_day = "Thu"; } break;
        case 5: { week_day = "Fri"; } break;
        case 6: { week_day = "Sat"; } break;
        case 7: { week_day = "Sun"; } break;
    }
    RG_ASSERT(week_day);

    const char *month = nullptr;
    switch (date.month) {
        case 1: { month = "Jan"; } break;
        case 2: { month = "Feb"; } break;
        case 3: { month = "Mar"; } break;
        case 4: { month = "Apr"; } break;
        case 5: { month = "May"; } break;
        case 6: { month = "Jun"; } break;
        case 7: { month = "Jul"; } break;
        case 8: { month = "Aug"; } break;
        case 9: { month = "Sep"; } break;
        case 10: { month = "Oct"; } break;
        case 11: { month = "Nov"; } break;
        case 12: { month = "Dec"; } break;
    }
    RG_ASSERT(month);

    arg.type = FmtType::Buffer;
    Fmt(arg.u.buf, "%1, %2 %3 %4 %5:%6:%7 GMT",
                   week_day, date.day, month, date.year,
                   FmtArg(date.hour).Pad0(-2), FmtArg(date.min).Pad0(-2), FmtArg(date.sec).Pad0(-2));

    return arg;
}

bool s3_Session::Open(const char *url, const char *id, const char *key)
{
    RG_ASSERT(!open);

    CURLU *h = curl_url();
    RG_DEFER { curl_url_cleanup(h); };

    CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, 0);
    if (ret != CURLUE_OK) {
        LogError("Failed to parse URL '%1': %2", url, curl_url_strerror(ret));
        return false;
    }

    const char *scheme = GetUrlPart(h, CURLUPART_SCHEME, &str_alloc).ptr;
    Span<const char> host = GetUrlPart(h, CURLUPART_HOST, &str_alloc);
    const char *path = GetUrlPart(h, CURLUPART_PATH, &str_alloc).ptr;

    if (!host.len) {
        LogError("Invalid URL host in '%1'", url);
        return false;
    }

    if (path && !TestStr(path, "/")) {
        Span<const char> bucket = SplitStr(path + 1, '/');
        this->bucket = DuplicateString(bucket, &str_alloc).ptr;
    }
    if (EndsWith(host, ".amazonaws.com")) {
        Span<const char> remain = host;

        // Find bucket name
        if (!StartsWith(remain, "s3.")) {
            if (this->bucket) {
                LogError("Duplicate bucket name in S3 URL '%1'", url);
                return false;
            }

            Span<const char> part = SplitStr(remain, '.', &remain);
            this->bucket = DuplicateString(part, &str_alloc).ptr;
        }

        if (StartsWith(remain, "s3.")) {
            SplitStr(remain, '.', &remain);
        }
        if (remain != "amazonaws.com") {
            Span<const char> part = SplitStr(remain, '.');
            this->region = DuplicateString(part, &str_alloc).ptr;
        }
    } else {
        LogError("Invalid or incomplete S3 URL '%1' (no bucket name)", url);
        return false;
    }

    this->scheme = scheme;
    this->host = host.ptr;

    return OpenAccess(id, key);
}

bool s3_Session::Open(const char *host, const char *region, const char *bucket, const char *id, const char *key)
{
    RG_ASSERT(!open);

    if (!region) {
        region = getenv("AWS_REGION");
    }

    this->scheme = "https";
    this->host = DuplicateString(host, &str_alloc).ptr;
    this->region = region ? DuplicateString(region, &str_alloc).ptr : nullptr;
    this->bucket = bucket ? DuplicateString(bucket, &str_alloc).ptr : nullptr;

    return OpenAccess(id, key);
}

void s3_Session::Close()
{
    open = false;

    scheme = nullptr;
    host = nullptr;
    region = nullptr;
    bucket = nullptr;

    str_alloc.ReleaseAll();
}

bool s3_Session::ListObjects(const char *prefix, Allocator *alloc, HeapArray<const char *> *out_keys)
{
    BlockAllocator temp_alloc;

    RG_DEFER_NC(out_guard, len = out_keys->len) { out_keys->RemoveFrom(len); };

    CURL *curl = InitCurl();
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

        int status = PerformCurl(curl, "S3");
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

bool s3_Session::GetObject(Span<const char> key, Size max_len, HeapArray<uint8_t> *out_obj)
{
    BlockAllocator temp_alloc;

    RG_DEFER_NC(out_guard, len = out_obj->len) { out_obj->RemoveFrom(len); };

    CURL *curl = InitCurl();
    if (!curl)
        return false;
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
            return false;
        }
    }

    int status = PerformCurl(curl, "S3");
    if (status < 0)
        return false;
    if (status != 200) {
        LogError("Failed to get S3 object with status %1", status);
        return false;
    }

    out_guard.Disable();
    return true;
}

bool s3_Session::PutObject(Span<const char> key, Span<const uint8_t> data, const char *mimetype)
{
    BlockAllocator temp_alloc;

    CURL *curl = InitCurl();
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

    int status = PerformCurl(curl, "S3");
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

    CURL *curl = InitCurl();
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

    int status = PerformCurl(curl, "S3");
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

    // Access keys
    if (!id) {
        id = getenv("AWS_ACCESS_KEY_ID");

        if (!id) {
            LogError("Missing AWS_ACCESS_KEY_ID variable");
            return false;
        }

        access_id = DuplicateString(id, &str_alloc).ptr;
    }
    if (!key) {
        key = getenv("AWS_SECRET_ACCESS_KEY");

        if (!key) {
            LogError("Missing AWS_SECRET_ACCESS_KEY variable");
            return false;
        }

        access_key = DuplicateString(key, &str_alloc).ptr;
    }

    Span<const char> path;
    Span<const char> url = MakeURL({}, &temp_alloc, &path);

    // Determine region if needed
    if (!region && !DetermineRegion(url.ptr))
        return false;

    // Test access
    {
        CURL *curl = InitCurl();
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

        int status = PerformCurl(curl, "S3");
        if (status < 0)
            return false;
        if (status != 200 && status != 201) {
            LogError("Failed to authenticate to S3 bucket (%1)", status);
            return false;
        }
    }

    open = true;
    return true;
}

bool s3_Session::DetermineRegion(const char *url)
{
    RG_ASSERT(!open);

    CURL *curl = InitCurl();
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

    int status = PerformCurl(curl, "S3");
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
    out_headers[len++].data = Fmt(alloc, "x-amz-date: %1", FormatRfcDate(date)).ptr;
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
        buf.len += Fmt(buf.TakeAvailable(), "host:%1\nx-amz-content-sha256:%2\nx-amz-date:%3\n\n", host, FormatSha256(sha256), FormatRfcDate(date)).len;
        buf.len += Fmt(buf.TakeAvailable(), "host;x-amz-content-sha256;x-amz-date\n").len;
        buf.len += Fmt(buf.TakeAvailable(), "%1", FormatSha256(sha256)).len;

        crypto_hash_sha256(canonical, (const uint8_t *)buf.data, (size_t)buf.len);
    }

    // Create string to sign
    LocalArray<char, 4096> string;
    {
        string.len += Fmt(string.TakeAvailable(), "AWS4-HMAC-SHA256\n").len;
        string.len += Fmt(string.TakeAvailable(), "%1\n", FormatRfcDate(date)).len;
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

    if (bucket) {
        Fmt(&buf, "/%1", bucket);
    }
    if (key.len) {
        Fmt(&buf, "/%1", key);
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
