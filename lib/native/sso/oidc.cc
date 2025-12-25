// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/request/curl.hh"
#include "lib/native/wrap/json.hh"
#include "oidc.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#include "vendor/mbedtls/include/psa/crypto.h"

namespace K {

enum class JwtSigningAlgorithm {
    RS256,
    PS256,
    HS256
};
static const char *const JwtSigningAlgorithmNames[] = {
    "RS256",
    "PS256",
    "HS256"
};

struct JwksCacheID {
    const oidc_Provider *provider;
    const char *kid;
    psa_algorithm_t algorithm;

    bool operator==(const JwksCacheID &other) const
    {
        return provider == other.provider &&
               TestStr(kid, other.kid) &&
               algorithm == other.algorithm;
    }
    bool operator !=(const JwksCacheID &other) const { return !(*this == other); }

    uint64_t Hash() const
    {
        uint64_t hash = HashTraits<const void *>::Hash(provider) ^
                        HashTraits<const char *>::Hash(kid) ^
                        HashTraits<uint32_t>::Hash(algorithm);
        return hash;
    }
};

struct JwksCacheEntry {
    JwksCacheID id;
    psa_key_id_t key;

    K_HASHTABLE_HANDLER(JwksCacheEntry, id);
};

static const int TimestampTolerance = 120 * 1000; // 2 minutes
static const int JwksExpirationDelay = 6 * 3600 * 1000; // Fetch new JWKS files every 6 hours

static std::shared_mutex jwks_mutex;
static int64_t jwks_timestamp;
static BucketArray<JwksCacheEntry> jwks_entries;
static HashTable<JwksCacheID, const JwksCacheEntry *> jwks_map;
static HashSet<const void *> jwks_providers;
static HeapArray<psa_key_id_t> jwks_keys;
static HeapArray<psa_key_id_t> jwks_old_keys;

K_EXIT(ClearJwtKeys)
{
    std::lock_guard<std::shared_mutex> lock_excl(jwks_mutex);

    for (psa_key_id_t key: jwks_old_keys) {
        psa_destroy_key(key);
    }
    for (psa_key_id_t key: jwks_keys) {
        psa_destroy_key(key);
    }
}

bool oidc_Provider::Finalize(Allocator *alloc)
{
    const char *discover_url = Fmt(alloc, "%1/.well-known/openid-configuration", url).ptr;

    LogDebug("Fetching OIDC configuration from '%1'", discover_url);

    HeapArray<char> body;
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        K_DEFER { curl_easy_cleanup(curl); };

        curl_easy_setopt(curl, CURLOPT_URL, discover_url);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<char> *body = (HeapArray<char> *)udata;

            Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
            body->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

        int status = curl_Perform(curl, "fetch");

        if (status != 200) {
            if (status >= 0) {
                LogError("Failed to fetch OIDC configuration with status %1", status);
            }
            return false;
        }
    }

    // Parse configuration
    {
        StreamReader st(body.As<uint8_t>(), "<openid-configuration>");
        json_Parser json(&st, alloc);

        for (json.ParseObject(); json.InObject(); ) {
            Span<const char> key = json.ParseKey();

            if (key == "issuer") {
                json.ParseString(&issuer);
            } else if (key == "authorization_endpoint") {
                json.ParseString(&auth_url);
            } else if (key == "token_endpoint") {
                json.ParseString(&token_url);
            } else if (key == "jwks_uri") {
                json.ParseString(&jwks_url);
            } else {
                json.Skip();
            }
        }
        if (!json.IsValid())
            return false;
    }

    return true;
}

static bool CheckURL(const char *url)
{
    CURLU *h = curl_url();
    K_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME);

        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
        if (ret != CURLUE_OK) {
            LogError("Malformed OIDC URL '%1'", url);
            return false;
        }
    }

    // Check scheme
    {
        char *scheme = nullptr;

        CURLUcode ret = curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);
        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
        K_DEFER { curl_free(scheme); };

        if (scheme && !TestStr(scheme, "http") && !TestStr(scheme, "https")) {
            LogError("Unsupported OIDC scheme '%1'", scheme);
            return false;
        }
    }

    return true;
}

bool oidc_Provider::Validate() const
{
    bool valid = true;

    if (!title) {
        LogError("OIDC provider title is not set");
        valid = false;
    }

    if (url) {
        valid = CheckURL(url);
    } else {
        LogError("OIDC provider URL is not set");
        valid = false;
    }

    if (!client_id) {
        LogError("OIDC client ID is not set");
        valid = false;
    }
    if (!client_secret) {
        LogError("OIDC client secret is not set");
        valid = false;
    }

    if (!issuer) {
        LogError("Could not find OIDC issuer value");
        valid = false;
    }
    if (!auth_url) {
        LogError("Could not find OIDC authorization endpoint");
        valid = false;
    }
    if (!token_url) {
        LogError("Could not find OIDC token endpoint");
        valid = false;
    }
    if (!jwks_url) {
        LogError("Could not find OIDC JWKS endpoint");
        valid = false;
    }

    return valid;
}

bool oidc_ProviderSet::Validate() const
{
    bool valid = true;

    for (const oidc_Provider &provider: providers) {
        valid &= provider.Validate();
    }

    return valid;
}

bool oidc_LoadProviders(StreamReader *st, oidc_ProviderSet *out_set)
{
    oidc_ProviderSet set;

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("SSO config file must use sections");
                return false;
            }

            Allocator *alloc;
            oidc_Provider *provider = set.providers.AppendDefault(&alloc);

            provider->name = DuplicateString(prop.section, alloc).ptr;

            // Insert into map of providers
            {
                bool inserted;
                set.map.InsertOrGet(provider, &inserted);

                if (!inserted) {
                    LogError("Duplicate SSO provider '%1'", provider->name);
                    valid = false;
                }
            }

            do {
                if (prop.key == "Title") {
                    provider->title = DuplicateString(prop.value, alloc).ptr;
                } else if (prop.key == "URL") {
                    Span<const char> url = TrimStrRight(prop.value, '/');
                    provider->url = DuplicateString(url, alloc).ptr;
                } else if (prop.key == "ClientID") {
                    provider->client_id = DuplicateString(prop.value, alloc).ptr;
                } else if (prop.key == "ClientSecret") {
                    provider->client_secret = DuplicateString(prop.value, alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            valid &= provider->url && provider->Finalize(alloc);
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    std::swap(*out_set, set);
    return true;
}

bool oidc_LoadProviders(const char *filename, oidc_ProviderSet *out_set)
{
    StreamReader st(filename);
    return oidc_LoadProviders(&st, out_set);
}

static uint8_t *GetSsoCookieKey32()
{
    static uint8_t key[32];
    static std::once_flag flag;

    std::call_once(flag, []() {
        FillRandomSafe(key, K_SIZE(key));
    });

    return key;
}

void oidc_PrepareAuthorization(const oidc_Provider &provider, const char *scopes, const char *callback,
                               const char *redirect, Allocator *alloc, oidc_AuthorizationInfo *out_auth)
{
    BlockAllocator temp_alloc;

    Span<const char> state = Fmt(&temp_alloc, "%1|%2|%3", FmtRandom(32), provider.name, redirect);
    Span<const char> nonce = Fmt(&temp_alloc, "%1", FmtRandom(32));

    out_auth->url = Fmt(alloc, "%1?client_id=%2&redirect_uri=%3&scope=openid+%4&response_type=code&state=%5&nonce=%6",
                               provider.auth_url, FmtUrlSafe(provider.client_id, "-._~@"),
                               FmtUrlSafe(callback, "-._~@"), FmtUrlSafe(scopes, "-._~@"),
                               FmtUrlSafe(state, "-._~@"), FmtUrlSafe(nonce, "-._~@")).ptr;

    // Prepare encrypted cookie
    {
        Span<const char> secret = Fmt(&temp_alloc, "%1:%2", nonce, state);
        const uint8_t *key = GetSsoCookieKey32();

        Span<uint8_t> cypher = AllocateSpan<uint8_t>(&temp_alloc, crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + secret.len);
        FillRandomSafe(cypher.ptr, crypto_secretbox_NONCEBYTES);

        crypto_secretbox_easy(cypher.ptr + crypto_secretbox_NONCEBYTES, (const uint8_t *)secret.ptr, secret.len, cypher.ptr, key);

        Size needed = (Size)sodium_base64_encoded_len(cypher.len, sodium_base64_VARIANT_ORIGINAL);
        Span<char> base64 = AllocateSpan<char>(alloc, needed);
        sodium_bin2base64(base64.ptr, base64.len, cypher.ptr, (size_t)cypher.len, sodium_base64_VARIANT_ORIGINAL);

        out_auth->cookie = base64.ptr;
    }
}

bool oidc_CheckCookie(Span<const char> cookie, Span<const char> rnd, Allocator *alloc, oidc_CookieInfo *out_info)
{
    BlockAllocator temp_alloc;

    // Decrypt safety cookie
    Span<const char> state;
    Span<const char> nonce;
    {
        Span<uint8_t> cypher = AllocateSpan<uint8_t>(&temp_alloc, cookie.len);

        if (sodium_base642bin(cypher.ptr, cypher.len, cookie.ptr, (size_t)cookie.len,
                              nullptr, (size_t *)&cypher.len, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
            LogError("Malformed OIDC safety cookie");
            return false;
        }
        if (cypher.len < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
            LogError("Malformed OIDC safety cookie");
            return false;
        }

        Span<const char> secret = AllocateSpan<char>(&temp_alloc, cypher.len - crypto_secretbox_NONCEBYTES - crypto_secretbox_MACBYTES);
        const uint8_t *key = GetSsoCookieKey32();

        if (crypto_secretbox_open_easy((uint8_t *)secret.ptr, cypher.ptr + crypto_secretbox_NONCEBYTES,
                                       cypher.len - crypto_secretbox_NONCEBYTES, cypher.ptr, key) != 0) {
            LogError("Invalid OIDC safety cookie");
            return false;
        }

        nonce = SplitStr(secret, ':', &state);
    }

    // Quick rejection if state does not match
    if (state.len != rnd.len || sodium_memcmp(state.ptr, rnd.ptr, state.len) != 0) {
        LogError("Mismatched SSO state values");
        return false;
    }

    Span<const char> provider;
    Span<const char> redirect;
    {
        Span<const char> remain = state;

        // Skip random part
        SplitStr(remain, '|', &remain);

        provider = SplitStr(remain, '|', &remain);
        redirect = SplitStr(remain, '|', &remain);
    }

    out_info->provider = DuplicateString(provider, alloc).ptr;
    out_info->redirect = DuplicateString(redirect, alloc).ptr;
    out_info->nonce = DuplicateString(nonce, alloc).ptr;

    return true;
}

bool oidc_ExchangeCode(const oidc_Provider &provider, const char *callback_url, const char *code,
                      Allocator *alloc, oidc_TokenSet *out_set)
{
    BlockAllocator temp_alloc;

    Span<char> post = Fmt(&temp_alloc, "grant_type=authorization_code&client_id=%1&client_secret=%2&redirect_uri=%3&code=%4",
                                       FmtUrlSafe(provider.client_id, "-._~@"), FmtUrlSafe(provider.client_secret, "-._~@"),
                                       FmtUrlSafe(callback_url, "-._~@"), FmtUrlSafe(code, "-._~@"));

    // The URL may live on inside curl allocated memory but it's better than nothing
    K_DEFER { ZeroSafe(post.ptr, post.len); };

    HeapArray<char> body;
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        K_DEFER { curl_easy_cleanup(curl); };

        curl_easy_setopt(curl, CURLOPT_URL, provider.token_url);

        curl_slist header = { (char *)"Content-Type: application/x-www-form-urlencoded", nullptr };
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, &header);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.ptr);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post.len);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<char> *body = (HeapArray<char> *)udata;

            Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
            body->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

        int status = curl_Perform(curl, "fetch");

        if (status != 200) {
            if (status >= 0) {
                LogError("Failed to exchange OIDC code with status %1", status);
            }
            return false;
        }
    }

    oidc_TokenSet set;
    const char *type = nullptr;
    {
        StreamReader st(body.As<uint8_t>(), "<tokens>");
        json_Parser json(&st, alloc);

        for (json.ParseObject(); json.InObject(); ) {
            Span<const char> key = json.ParseKey();

            if (key == "token_type") {
                json.ParseString(&type);
            } else if (key == "id_token") {
                json.ParseString(&set.id);
            } else if (key == "access_token") {
                json.ParseString(&set.access);
            } else {
                json.Skip();
            }
        }
        if (!json.IsValid())
            return false;
    }

    if (!type || !TestStrI(type, "Bearer")) {
        LogError("Unsupported SSO token type");
        return false;
    }
    if (!set.id) {
        LogError("Missing SSO ID token");
        return false;
    }

    std::swap(*out_set, set);
    return true;
}

template <typename T>
static bool DecodeJwtFragment(Span<const T> str, Allocator *alloc, Span<const T> *out)
{
    Span<T> buf = AllocateSpan<T>(alloc, str.len);

    if (sodium_base642bin((uint8_t *)buf.ptr, buf.len, (const char *)str.ptr, str.len, nullptr, (size_t *)&buf.len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0) {
        LogError("Invalid JWT fragment");
        return false;
    }

    *out = buf;
    return true;
}

// Must be called with exclusive jwks_mutex lock
static bool ImportRsaSigningKey(Span<const char> n, Span<const char> e, psa_key_id_t *out_rs256, psa_key_id_t *out_ps256)
{
    LocalArray<uint8_t, 1024> modulo;
    LocalArray<uint8_t, 32> exponent;
    if (sodium_base642bin(modulo.data, K_SIZE(modulo.data), n.ptr, (size_t)n.len, nullptr, (size_t *)&modulo.len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0) {
        LogError("Failed to decode RSA key modulus");
        return false;
    }
    if (sodium_base642bin(exponent.data, K_SIZE(exponent.data), e.ptr, (size_t)e.len, nullptr, (size_t *)&exponent.len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0) {
        LogError("Failed to decode RSA key exponent");
        return false;
    }

    LocalArray<uint8_t, 4096> der;

    // Assemble DER key data because that's what the the PSA code wants to import an RSA key ><
    der.Append(0x30);
    der.Append(0x82);
    der.Append((uint8_t)((8 + modulo.len + exponent.len) >> 8));
    der.Append((uint8_t)((8 + modulo.len + exponent.len) & 0xF));
    der.Append(0x02);
    der.Append(0x82);
    der.Append((uint8_t)((1 + modulo.len) >> 8));
    der.Append((uint8_t)((1 + modulo.len) & 0xF));
    der.Append(0);
    der.Append(modulo);
    der.Append(0x02);
    der.Append((uint8_t)((1 + exponent.len) & 0xF));
    der.Append(0);
    der.Append(exponent);

    // Import for RS256 algorithm
    {
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
        psa_set_key_algorithm(&attributes, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));

        if (int ret = psa_import_key(&attributes, der.data, (size_t)der.len, out_rs256); ret != PSA_SUCCESS) {
            LogError("Failed to import JWK public RSA key: error %1", ret);
            return false;
        }

        jwks_keys.Append(*out_rs256);
    }

    // Import for PS256 algorithm
    {
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
        psa_set_key_algorithm(&attributes, PSA_ALG_RSA_PSS(PSA_ALG_SHA_256));

        if (int ret = psa_import_key(&attributes, der.data, (size_t)der.len, out_ps256); ret != PSA_SUCCESS) {
            LogError("Failed to import JWK public RSA key: error %1", ret);
            return false;
        }

        jwks_keys.Append(*out_ps256);
    }

    return true;
}

static psa_key_id_t FetchJwksKey(const oidc_Provider &provider, const char *kid, psa_algorithm_t algorithm)
{
    int64_t now = GetUnixTime();

    // Fast path
    {
        std::shared_lock<std::shared_mutex> lock_shr(jwks_mutex);

        if (now - jwks_timestamp < JwksExpirationDelay) {
            const JwksCacheID id = { &provider, kid, algorithm };
            const JwksCacheEntry *entry = jwks_map.FindValue(id, nullptr);

            if (entry)
                return entry->key;
            if (jwks_providers.Find(&provider))
                return PSA_KEY_ID_NULL;
        }
    }

    BlockAllocator temp_alloc;

    std::lock_guard<std::shared_mutex> lock_excl(jwks_mutex);

    // Regularly refresh keysets, they are not static!
    // We keep the old keys for another cycle because they might be in use somewhere... but after 24 hours,
    // they will definitely be forgotten and we'll be good to go!
    if (now - jwks_timestamp < JwksExpirationDelay) {
        jwks_entries.Clear();
        jwks_providers.Clear();

        for (psa_key_id_t key: jwks_old_keys) {
            psa_destroy_key(key);
        }
        std::swap(jwks_keys, jwks_old_keys);
        jwks_keys.Clear();

        jwks_timestamp = now;
    }

    // Fetch JWKS for this provider
    {
        LogDebug("Fetching OIDC JWKS file from '%1'", provider.jwks_url);

        Size prev_count = jwks_entries.count;

        K_DEFER_N(err_guard) {
            jwks_entries.RemoveFrom(prev_count);
            jwks_providers.Remove(&provider);
        };

        jwks_providers.Set(&provider);

        HeapArray<char> body;
        {
            CURL *curl = curl_Init();
            if (!curl)
                return PSA_KEY_ID_NULL;
            K_DEFER { curl_easy_cleanup(curl); };

            curl_easy_setopt(curl, CURLOPT_URL, provider.jwks_url);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
                HeapArray<char> *body = (HeapArray<char> *)udata;

                Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
                body->Append(buf);

                return nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

            int status = curl_Perform(curl, "fetch");

            if (status != 200) {
                if (status >= 0) {
                    LogError("Failed to fetch OIDC JWKS with status %1", status);
                }
                return PSA_KEY_ID_NULL;
            }
        }

        // Parse JSON
        {
            StreamReader st(body.As<uint8_t>(), "<jwks>");
            json_Parser json(&st, &temp_alloc);

            for (json.ParseObject(); json.InObject(); ) {
                Span<const char> key = json.ParseKey();

                if (key == "keys") {
                    for (json.ParseArray(); json.InArray(); ) {
                        const char *kid = nullptr;
                        const char *kty = nullptr;
                        const char *use = nullptr;
                        const char *n = nullptr;
                        const char *e = nullptr;

                        for (json.ParseObject(); json.InObject(); ) {
                            Span<const char> key = json.ParseKey();

                            if (key == "kid") {
                                json.ParseString(&kid);
                            } else if (key == "kty") {
                                json.ParseString(&kty);
                            } else if (key == "use") {
                                json.ParseString(&use);
                            } else if (key == "n") {
                                json.ParseString(&n);
                            } else if (key == "e") {
                                json.ParseString(&e);
                            } else {
                                json.Skip();
                            }
                        }

                        if (!kid || !kty || !use)
                            continue;
                        if (!TestStr(use, "sig"))
                            continue;

                        if (TestStr(kty, "RSA")) {
                            if (!n || !e)
                                continue;

                            // In theory, there's an alg field in the JWKS entry. But in practice, at least
                            // with several providers, it is RS256 even when PS256 is used. So just make two keys,
                            // one for each algorithm.

                            psa_key_id_t rs256;
                            psa_key_id_t ps256;
                            if (!ImportRsaSigningKey(n, e, &rs256, &ps256))
                                continue;

                            // Create entry for RS256 algorithm
                            {
                                Allocator *alloc;
                                JwksCacheEntry *entry = jwks_entries.AppendDefault(&alloc);

                                entry->id = { &provider, DuplicateString(kid, alloc).ptr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256) };
                                entry->key = rs256;
                            }

                            // Create entry for PS256 algorithm
                            {
                                Allocator *alloc;
                                JwksCacheEntry *entry = jwks_entries.AppendDefault(&alloc);

                                entry->id = { &provider, DuplicateString(kid, alloc).ptr, PSA_ALG_RSA_PSS(PSA_ALG_SHA_256) };
                                entry->key = ps256;
                            }
                        } else {
                            continue;
                        }
                    }
                } else {
                    json.Skip();
                }
            }
            if (!json.IsValid())
                return PSA_KEY_ID_NULL;
        }

        for (Size i = prev_count; i < jwks_entries.count; i++) {
            const JwksCacheEntry *entry = &jwks_entries[i];
            jwks_map.Set(entry);
        }

        err_guard.Disable();
    }

    const JwksCacheID id = { &provider, kid, algorithm };
    const JwksCacheEntry *entry = jwks_map.FindValue(id, nullptr);

    if (!entry) {
        LogError("Unknown JWT key with KID '%1' (%2)", kid, provider.name);
        return PSA_KEY_ID_NULL;
    }
    if (entry->key == PSA_KEY_ID_NULL) {
        LogError("Cannot verify JWT key with KID '%1' (%2)", kid, provider.name);
        return PSA_KEY_ID_NULL;
    }

    return entry->key;
}

static void HmacSha256(Span<const uint8_t> key, Span<const uint8_t> message, uint8_t out_digest[32])
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
        crypto_hash_sha256_update(&state, message.ptr, (size_t)message.len);
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

bool oidc_DecodeIdToken(const oidc_Provider &provider, Span<const char> token, Span<const char> nonce,
                        Allocator *alloc, oidc_IdentityInfo *out_identity)
{
    BlockAllocator temp_alloc;

    Span<const char> header;
    Span<const char> payload;
    Span<const uint8_t> signature;
    Span<const uint8_t> siginput;
    {
        Span<const char> remain = token;

        header = SplitStr(remain, '.', &remain);
        payload = SplitStr(remain, '.', &remain);
        signature = remain.As<const uint8_t>();

        if (!header.len || !payload.len || !signature.len) {
            LogError("Invalid or empty JWT fragments");
            return false;
        }

        siginput = MakeSpan((const uint8_t *)token.ptr, payload.end() - header.ptr);
    }

    // Decode base64 fragments
    if (!DecodeJwtFragment(header, &temp_alloc, &header))
        return false;
    if (!DecodeJwtFragment(payload, &temp_alloc, &payload))
        return false;
    if (!DecodeJwtFragment(signature, &temp_alloc, &signature))
        return false;

    // Decode and check header
    JwtSigningAlgorithm algorithm;
    const char *kid = nullptr;
    {
        const char *type = nullptr;
        const char *alg = nullptr;

        // Parse JSON
        {
            StreamReader st(header.As<const uint8_t>(), "<jwt>");
            json_Parser json(&st, &temp_alloc);

            for (json.ParseObject(); json.InObject(); ) {
                Span<const char> key = json.ParseKey();

                if (key == "typ") {
                    json.ParseString(&type);
                } else if (key == "alg") {
                    json.ParseString(&alg);
                } else if (key == "kid") {
                    json.ParseString(&kid);
                } else {
                    json.Skip();
                }
            }
            if (!json.IsValid())
                return false;
        }

        if (!type || !TestStrI(type, "JWT")) {
            LogError("Invalid JWT type '%1'", type);
            return false;
        }
        if (alg) {
            if (!OptionToEnum(JwtSigningAlgorithmNames, alg, &algorithm)) {
                LogError("Unsupported JWT signing algorithm '%1'", alg);
                return false;
            }
        } else {
            LogError("Missing JWT algorithm");
            return false;
        }

        switch (algorithm) {
            case JwtSigningAlgorithm::RS256:
            case JwtSigningAlgorithm::PS256: {
                if (!kid) {
                    LogError("Missing JWT signing key KID");
                    return false;
                }
            } break;

            case JwtSigningAlgorithm::HS256: {} break;
        }
    }

    // Check signature
    switch (algorithm) {
        case JwtSigningAlgorithm::RS256: {
            K_ASSERT(kid);

            psa_key_id_t key = FetchJwksKey(provider, kid, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));

            if (key == PSA_KEY_ID_NULL)
                return false;

            int ret = psa_verify_message(key, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256),
                                         siginput.ptr, (size_t)siginput.len,
                                         signature.ptr, (size_t)signature.len);

            if (ret != PSA_SUCCESS) {
                LogError("Failed JWT RS256 signature verification");
                return false;
            }
        } break;

        case JwtSigningAlgorithm::PS256: {
            K_ASSERT(kid);

            psa_key_id_t key = FetchJwksKey(provider, kid, PSA_ALG_RSA_PSS(PSA_ALG_SHA_256));

            if (key == PSA_KEY_ID_NULL)
                return false;

            int ret = psa_verify_message(key, PSA_ALG_RSA_PSS(PSA_ALG_SHA_256),
                                         siginput.ptr, (size_t)siginput.len,
                                         signature.ptr, (size_t)signature.len);

            if (ret != PSA_SUCCESS) {
                LogError("Failed JWT PS256 signature verification");
                return false;
            }
        } break;

        case JwtSigningAlgorithm::HS256: {
            Span<const uint8_t> key = MakeSpan((const uint8_t *)provider.client_secret, strlen(provider.client_secret));

            if (signature.len != 32) {
                LogError("Invalid JWT HS256 signature length");
                return false;
            }

            uint8_t hmac[32];
            HmacSha256(key, siginput, hmac);

            if (sodium_memcmp(hmac, signature.ptr, 32) != 0) {
                LogError("Failed JWT HS256 signature verification");
                return false;
            }
        } break;
    }

    int64_t now = GetUnixTime();

    // Decode and check payload
    oidc_IdentityInfo identity = {};
    {
        int64_t iat = -1;
        int64_t exp = -1;
        const char *iss = nullptr;
        const char *aud = nullptr;
        Span<const char> nonce2 = {};

        // Parse JSON
        {
            StreamReader st(payload.As<const uint8_t>(), "<jwt>");
            json_Parser json(&st, alloc);

            for (json.ParseObject(); json.InObject(); ) {
                Span<const char> key = json.ParseKey();

                if (key == "iat") {
                    json.ParseInt(&iat);
                } else if (key == "exp") {
                    json.ParseInt(&exp);
                } else if (key == "iss") {
                    json.ParseString(&iss);
                } else if (key == "aud") {
                    json.ParseString(&aud);
                } else if (key == "nonce") {
                    json.ParseString(&nonce2);
                } else if (key == "sub") {
                    json.ParseString(&identity.sub);
                } else if (key == "email") {
                    json.ParseString(&identity.email);
                } else if (key == "email_verified") {
                    json.ParseBool(&identity.email_verified);
                } else if (json.PeekToken() == json_TokenType::String) {
                    const char *str = json.ParseString().ptr;
                    identity.attributes.Set(key.ptr, str);
                } else {
                    json.Skip();
                }
            }
            if (!json.IsValid())
                return false;
        }

        if (iat < 0 || exp < 0 || !iss || !aud || !nonce2.ptr || !identity.sub) {
            LogError("Missing or invalid JWT payload values");
            return false;
        }

        if (iat > (now + TimestampTolerance) / 1000) {
            LogError("Cannot use JWT token with future issue timestamp");
            return false;
        }
        if (exp < (now - TimestampTolerance) / 1000) {
            LogError("Cannot use expired JWT token");
            return false;
        }
        if (!TestStr(iss, provider.issuer)) {
            LogError("JWT issuer mismatch with OIDC configuration");
            return false;
        }
        if (!TestStr(aud, provider.client_id)) {
            LogError("JWT client ID mismatch with OIDC configuration");
            return false;
        }

        if (nonce2.len != nonce.len || sodium_memcmp(nonce2.ptr, nonce.ptr, nonce.len) != 0) {
            LogError("Invalid OIDC nonce in JWT payload");
            return false;
        }

        identity.email_verified &= !!identity.email;
    }

    std::swap(*out_identity, identity);
    return true;
}

}
