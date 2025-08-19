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

#include "src/core/base/base.hh"
#include "key.hh"
#include "priv_key.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif

namespace RG {

static void SeedSigningPair(const uint8_t sk[32], uint8_t pk[32])
{
    uint8_t hash[64];
    RG_DEFER { ZeroSafe(hash, RG_SIZE(hash)); };

    crypto_hash_sha512(hash, sk, 32);
    crypto_scalarmult_ed25519_base(pk, hash);
}

bool rk_LoadKeys(Span<const uint8_t> raw, rk_KeySet *out_keys)
{
    if (raw.len == rk_MasterKeySize) {
        out_keys->modes = UINT_MAX;
        out_keys->type = rk_KeyType::Master;

        crypto_kdf_blake2b_derive_from_key(out_keys->keys.ckey, RG_SIZE(out_keys->keys.ckey), (int)MasterDerivation::ConfigKey, DerivationContext, raw.ptr);
        crypto_kdf_blake2b_derive_from_key(out_keys->keys.dkey, RG_SIZE(out_keys->keys.dkey), (int)MasterDerivation::DataKey, DerivationContext, raw.ptr);
        crypto_kdf_blake2b_derive_from_key(out_keys->keys.lkey, RG_SIZE(out_keys->keys.lkey), (int)MasterDerivation::LogKey, DerivationContext, raw.ptr);
        crypto_kdf_blake2b_derive_from_key(out_keys->keys.nkey, RG_SIZE(out_keys->keys.nkey), (int)MasterDerivation::NeutralKey, DerivationContext, raw.ptr);
        SeedSigningPair(out_keys->keys.ckey, out_keys->keys.akey);
        crypto_scalarmult_curve25519_base(out_keys->keys.wkey, out_keys->keys.dkey);
        crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);

        MemCpy(out_keys->keys.skey, out_keys->keys.nkey, RG_SIZE(out_keys->keys.skey));
        SeedSigningPair(out_keys->keys.skey, out_keys->keys.pkey);
    } else if (raw.len == RG_SIZE(KeyData)) {
        const KeyData *data = (const KeyData *)raw.ptr;

        if (memcmp(data->prefix, "RKK01", 5)) {
            LogError("Invalid keyfile prefix");
            return false;
        }
        if (data->type <= 0 || data->type >= RG_LEN(rk_KeyTypeNames)) {
            LogError("Invalid key type %1", data->type);
            return false;
        }

        rk_KeyType type = (rk_KeyType)data->type;

        out_keys->type = type;
        MemCpy(out_keys->kid, data->kid, RG_SIZE(out_keys->kid));
        MemCpy(&out_keys->keys, &data->keys, RG_SIZE(out_keys->keys));

        switch (type) {
            case rk_KeyType::Master: { RG_UNREACHABLE(); } break;

            case rk_KeyType::WriteOnly: {
                out_keys->modes = (int)rk_AccessMode::Write;

                ZeroSafe(out_keys->keys.ckey, RG_SIZE(out_keys->keys.ckey));
                ZeroSafe(out_keys->keys.dkey, RG_SIZE(out_keys->keys.dkey));
                ZeroSafe(out_keys->keys.lkey, RG_SIZE(out_keys->keys.lkey));
            } break;

            case rk_KeyType::ReadWrite: {
                out_keys->modes = (int)rk_AccessMode::Read |
                                  (int)rk_AccessMode::Write |
                                  (int)rk_AccessMode::Log;

                ZeroSafe(out_keys->keys.ckey, RG_SIZE(out_keys->keys.ckey));
                crypto_scalarmult_curve25519_base(out_keys->keys.wkey, out_keys->keys.dkey);
                crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);
            } break;

            case rk_KeyType::LogOnly: {
                out_keys->modes = (int)rk_AccessMode::Log;

                ZeroSafe(out_keys->keys.ckey, RG_SIZE(out_keys->keys.ckey));
                ZeroSafe(out_keys->keys.dkey, RG_SIZE(out_keys->keys.dkey));
                ZeroSafe(out_keys->keys.wkey, RG_SIZE(out_keys->keys.wkey));
                crypto_scalarmult_curve25519_base(out_keys->keys.tkey, out_keys->keys.lkey);
            } break;
        }

        SeedSigningPair(out_keys->keys.skey, out_keys->keys.pkey);
    }

#if 0
    PrintLn("ckey = %1", FmtSpan(out_keys->keys.ckey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("akey = %1", FmtSpan(out_keys->keys.akey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("dkey = %1", FmtSpan(out_keys->keys.dkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("wkey = %1", FmtSpan(out_keys->keys.wkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("lkey = %1", FmtSpan(out_keys->keys.lkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("tkey = %1", FmtSpan(out_keys->keys.tkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("nkey = %1", FmtSpan(out_keys->keys.nkey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("skey = %1", FmtSpan(out_keys->keys.skey, FmtType::BigHex, "").Pad0(-2));
    PrintLn("pkey = %1", FmtSpan(out_keys->keys.pkey, FmtType::BigHex, "").Pad0(-2));
#endif

    return true;
}

bool rk_LoadKeys(const char *filename, rk_KeySet *out_keys)
{
    Span<uint8_t> raw = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
    RG_DEFER_C(len = raw.len) { ReleaseSafe(raw.ptr, len); };

    raw.len = ReadFile(filename, raw);
    if (raw.len < 0)
        return false;

    return rk_LoadKeys(raw, out_keys);
}

Size rk_DeriveKeys(const rk_KeySet &keys, rk_KeyType type, Span<uint8_t> out_raw)
{
    RG_ASSERT(keys.modes == UINT_MAX);
    RG_ASSERT(out_raw.len >= rk_MaximumKeySize);

    if (out_raw.len < RG_SIZE(KeyData)) {
        LogError("Key derivation requires at least %1 bytes", RG_SIZE(KeyData));
        return -1;
    }

    KeyData *data = (KeyData *)out_raw.ptr;
    ZeroSafe(data, RG_SIZE(*data));

    MemCpy(data->prefix, "RKK01", 5);
    randombytes_buf(data->kid, RG_SIZE(data->kid));
    data->type = (int8_t)type;

    switch (type) {
        case rk_KeyType::Master: {
            LogError("Cannot generate Master keys");
            return -1;
        } break;

        case rk_KeyType::WriteOnly: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, RG_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, wkey), keys.keys.wkey, RG_SIZE(keys.keys.wkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, tkey), keys.keys.tkey, RG_SIZE(keys.keys.tkey));
        } break;

        case rk_KeyType::ReadWrite: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, RG_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, dkey), keys.keys.dkey, RG_SIZE(keys.keys.dkey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, lkey), keys.keys.lkey, RG_SIZE(keys.keys.lkey));
        } break;

        case rk_KeyType::LogOnly: {
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, akey), keys.keys.akey, RG_SIZE(keys.keys.akey));
            MemCpy(data->keys + offsetof(rk_KeySet::Keys, lkey), keys.keys.lkey, RG_SIZE(keys.keys.lkey));
        } break;
    }

    randombytes_buf(data->keys + offsetof(rk_KeySet::Keys, skey), RG_SIZE(keys.keys.skey));

    // Sign serialized keyset to detect tampering
    crypto_sign_ed25519_detached(data->sig, nullptr, (const uint8_t *)data, offsetof(KeyData, sig), keys.keys.ckey);

    return RG_SIZE(KeyData);
}

bool rk_DeriveKeys(const rk_KeySet &keys, rk_KeyType type, const char *filename)
{
    Span<uint8_t> raw = MakeSpan((uint8_t *)AllocateSafe(rk_MaximumKeySize), rk_MaximumKeySize);
    RG_DEFER_C(len = raw.len) { ReleaseSafe(raw.ptr, len); };

    raw.len = rk_DeriveKeys(keys, type, raw);
    if (raw.len < 0)
        return false;

    if (!WriteFile(raw, filename, (int)StreamWriterFlag::NoBuffer))
        return false;
#if !defined(_WIN32)
    chmod(filename, 0600);
#endif

    return true;
}

}
