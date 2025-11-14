// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "key.hh"
#include "priv_key.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif

namespace K {

static const char *BeginPEM = "-----BEGIN REKKORD KEY-----";
static const char *EndPEM = "-----END REKKORD KEY-----";

static const Size EncodedLimit = 16384;
static const Size EncodedLineSplit = 70;

static Size DecodePEM(const char *filename, Span<const char> pem, Span<uint8_t> out_key)
{
    SplitStr(pem, BeginPEM, &pem);
    pem = SplitStr(pem, EndPEM);
    pem = TrimStr(pem);

    if (!pem.len) {
        LogError("Cannot find valid repository key in '%1'", filename);
        return -1;
    }
    if (pem.len > EncodedLimit) {
        LogError("Excessive base64 key size in '%1'", filename);
        return -1;
    }

    LocalArray<char, EncodedLimit> base64;
    for (char c: pem) {
        base64.data[base64.len] = c;
        base64.len += !IsAsciiWhite(c);
    }

    size_t len = 0;
    if (sodium_base642bin(out_key.ptr, out_key.len, base64.data, base64.len, nullptr,
                          &len, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
        LogError("Failed to decode base64 key");
        return -1;
    }

    return (Size)len;
}

static Size EncodePEM(Span<const uint8_t> key, Span<uint8_t> out_buf)
{
    Span<char> out = out_buf.As<char>();
    Size len = 0;

    LocalArray<char, EncodedLimit> base64;
    {
        Size encoded = sodium_base64_ENCODED_LEN(key.len, sodium_base64_VARIANT_ORIGINAL);
        if (encoded > K_SIZE(base64))
            goto error;
        base64.len = encoded - 1;

        sodium_bin2base64(base64.data, K_SIZE(base64.data), key.ptr, key.len, sodium_base64_VARIANT_ORIGINAL);
    }

    len += Fmt(out.Take(len, out.len - len), "%1\n", BeginPEM).len;
    for (Size i = 0; i < base64.len; i += EncodedLineSplit) {
        Size take = std::min(EncodedLineSplit, base64.len - i);
        len += Fmt(out.Take(len, out.len - len), "%1\n", base64.Take(i, take)).len;
    }
    len += Fmt(out.Take(len, out.len - len), "%1\n", EndPEM).len;

    if (len >= out.len)
        goto error;

    return len;

error:
    LogError("Failed to encode key to PEM string");
    return -1;
}

Size rk_ReadRawKey(const char *filename, Span<uint8_t> out_raw)
{
    Span<uint8_t> buf = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
    K_DEFER_C(len = buf.len) { ReleaseSafe(buf.ptr, len); };

    buf.len = ReadFile(filename, buf);
    if (buf.len < 0)
        return -1;

    Span<const char> pem = buf.As<const char>();
    return DecodePEM(filename, pem, out_raw);
}

bool rk_SaveRawKey(Span<const uint8_t> raw, const char *filename)
{
    Span<uint8_t> buf = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
    K_DEFER_C(len = buf.len) { ReleaseSafe(buf.ptr, len); };

    buf.len = EncodePEM(raw, buf);
    if (buf.len < 0)
        return false;

    if (!WriteFile(buf, filename, (int)StreamWriterFlag::NoBuffer))
        return false;
#if !defined(_WIN32)
    chmod(filename, 0600);
#endif

    return true;
}

static void SeedSigningPair(const uint8_t sk[32], uint8_t pk[32])
{
    uint8_t hash[64];
    K_DEFER { ZeroSafe(hash, K_SIZE(hash)); };

    crypto_hash_sha512(hash, sk, 32);
    crypto_scalarmult_ed25519_base(pk, hash);
}

bool rk_DeriveMasterKey(Span<const uint8_t> mkey, rk_KeySet *out_keys)
{
    if (mkey.len != rk_MasterKeySize) {
        LogError("Unexpected master key size");
        return false;
    }

    out_keys->modes = UINT_MAX;
    out_keys->type = rk_KeyType::Master;

    crypto_kdf_blake2b_derive_from_key(out_keys->keys.ckey, K_SIZE(out_keys->keys.ckey), (int)MasterDerivation::ConfigKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(out_keys->keys.dkey, K_SIZE(out_keys->keys.dkey), (int)MasterDerivation::DataKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(out_keys->keys.lkey, K_SIZE(out_keys->keys.lkey), (int)MasterDerivation::LogKey, DerivationContext, mkey.ptr);
    crypto_kdf_blake2b_derive_from_key(out_keys->keys.nkey, K_SIZE(out_keys->keys.nkey), (int)MasterDerivation::NeutralKey, DerivationContext, mkey.ptr);
    SeedSigningPair(out_keys->keys.ckey, out_keys->keys.akey);
    crypto_scalarmult_curve25519_base(out_keys->keys.wkey, out_keys->keys.dkey);
    crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);
    SeedSigningPair(out_keys->keys.nkey, out_keys->keys.vkey);

    MemCpy(out_keys->keys.skey, out_keys->keys.nkey, 32);
    MemCpy(out_keys->keys.pkey, out_keys->keys.vkey, 32);

    return true;
}

static bool DecodeKeyData(const KeyData &data, rk_KeySet *out_keys)
{
    if (memcmp(data.prefix, "RKK01", 5)) {
        LogError("Invalid keyfile prefix");
        return false;
    }
    if (data.badge.type <= 0 || data.badge.type >= K_LEN(rk_KeyTypeNames)) {
        LogError("Invalid key type %1", data.badge.type);
        return false;
    }

    rk_KeyType type = (rk_KeyType)data.badge.type;

    out_keys->type = type;
    MemCpy(out_keys->kid, data.badge.kid, K_SIZE(out_keys->kid));
    MemCpy(&out_keys->keys, &data.keys, K_SIZE(out_keys->keys));
    MemCpy(out_keys->badge, &data.badge, K_SIZE(out_keys->badge));

    switch (type) {
        case rk_KeyType::Master: { K_UNREACHABLE(); } break;

        case rk_KeyType::WriteOnly: {
            out_keys->modes = (int)rk_AccessMode::Write;

            ZeroSafe(out_keys->keys.ckey, K_SIZE(out_keys->keys.ckey));
            ZeroSafe(out_keys->keys.dkey, K_SIZE(out_keys->keys.dkey));
            ZeroSafe(out_keys->keys.lkey, K_SIZE(out_keys->keys.lkey));
        } break;

        case rk_KeyType::ReadWrite: {
            out_keys->modes = (int)rk_AccessMode::Read |
                              (int)rk_AccessMode::Write |
                              (int)rk_AccessMode::Log;

            ZeroSafe(out_keys->keys.ckey, K_SIZE(out_keys->keys.ckey));
            crypto_scalarmult_curve25519_base(out_keys->keys.wkey, out_keys->keys.dkey);
            crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);
        } break;

        case rk_KeyType::LogOnly: {
            out_keys->modes = (int)rk_AccessMode::Log;

            ZeroSafe(out_keys->keys.ckey, K_SIZE(out_keys->keys.ckey));
            ZeroSafe(out_keys->keys.dkey, K_SIZE(out_keys->keys.dkey));
            ZeroSafe(out_keys->keys.wkey, K_SIZE(out_keys->keys.wkey));
            crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);
        } break;
    }

    SeedSigningPair(out_keys->keys.skey, out_keys->keys.pkey);

    return true;
}

bool rk_LoadKeyFile(const char *filename, rk_KeySet *out_keys)
{
    Span<uint8_t> raw = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
    K_DEFER_C(len = raw.len) { ReleaseSafe(raw.ptr, len); };

    raw.len = rk_ReadRawKey(filename, raw);
    if (raw.len < 0)
        return false;

    if (raw.len == rk_MasterKeySize) {
        return rk_DeriveMasterKey(raw, out_keys);
    } else if (raw.len == K_SIZE(KeyData)) {
        const KeyData *data = (const KeyData *)raw.ptr;
        return DecodeKeyData(*data, out_keys);
    }

    LogError("Malformed key file");
    return false;
}

bool rk_ExportKeyFile(const rk_KeySet &keys, rk_KeyType type, const char *filename, rk_KeySet *out_keys)
{
    K_ASSERT(keys.type == rk_KeyType::Master);
    K_ASSERT(keys.modes == UINT_MAX);

    KeyData *data = (KeyData *)AllocateSafe(K_SIZE(KeyData));
    K_DEFER { ReleaseSafe(data, K_SIZE(KeyData)); };

    MemCpy(data->prefix, "RKK01", 5);
    FillRandomSafe(data->badge.kid, K_SIZE(data->badge.kid));
    data->badge.type = (int8_t)type;

    switch (type) {
        case rk_KeyType::Master: {
            LogError("Cannot generate Master key");
            return false;
        } break;

        case rk_KeyType::WriteOnly: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, K_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, wkey), keys.keys.wkey, K_SIZE(keys.keys.wkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, tkey), keys.keys.tkey, K_SIZE(keys.keys.tkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, vkey), keys.keys.vkey, K_SIZE(keys.keys.vkey));
        } break;

        case rk_KeyType::ReadWrite: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, K_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, dkey), keys.keys.dkey, K_SIZE(keys.keys.dkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, lkey), keys.keys.lkey, K_SIZE(keys.keys.lkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, vkey), keys.keys.vkey, K_SIZE(keys.keys.vkey));
        } break;

        case rk_KeyType::LogOnly: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, K_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, lkey), keys.keys.lkey, K_SIZE(keys.keys.lkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, vkey), keys.keys.vkey, K_SIZE(keys.keys.vkey));
        } break;
    }

    FillRandomSafe(data->keys + offsetof(rk_KeySet::Keys, skey), K_SIZE(keys.keys.skey));
    SeedSigningPair(data->keys + offsetof(rk_KeySet::Keys, skey), data->badge.pkey);

    // Sign serialized keyset to detect tampering
    crypto_sign_ed25519_detached(data->badge.sig, nullptr, (const uint8_t *)&data->badge, offsetof(KeyData::Badge, sig), keys.keys.nkey);
    crypto_sign_ed25519_detached(data->sig, nullptr, (const uint8_t *)data, offsetof(KeyData, sig), keys.keys.nkey);

    // Export to file
    {
        Span<const uint8_t> raw = MakeSpan((const uint8_t *)data, K_SIZE(*data));

        if (!rk_SaveRawKey(raw, filename))
            return false;
    }

    if (out_keys) {
        bool success = DecodeKeyData(*data, out_keys);
        K_ASSERT(success);
    }

    return true;
}

}
