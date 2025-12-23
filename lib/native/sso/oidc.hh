// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct oidc_Provider {
    const char *name = nullptr;

    const char *url = nullptr;
    const char *client_id = nullptr;
    const char *client_secret = nullptr;

    // Auto-discovery
    const char *issuer = nullptr;
    const char *auth_url = nullptr;
    const char *token_url = nullptr;
    const char *jwks_url = nullptr;

    bool Finalize(Allocator *alloc);

    bool Validate() const;

    K_HASHTABLE_HANDLER(oidc_Provider, name);
};

struct oidc_ProviderSet {
    BucketArray<oidc_Provider> providers;
    HashTable<const char *, const oidc_Provider *> map;

    bool Validate() const;
};

struct oidc_AuthorizationInfo {
    const char *url = nullptr;
    const char *cookie = nullptr;
};

struct oidc_CookieInfo {
    const char *provider = nullptr;
    const char *redirect = nullptr;
    Span<const char> nonce = {};
};

struct oidc_TokenSet {
    const char *id = nullptr;
    const char *access = nullptr; // Can be NULL
};

struct oidc_IdentityInfo {
    const char *sub = nullptr;
    const char *email = nullptr; // Can be NULL
    bool verified = false;
};

// You must finalize the set once every provider is loaded
bool oidc_LoadProviders(StreamReader *st, oidc_ProviderSet *out_set);
bool oidc_LoadProviders(const char *filename, oidc_ProviderSet *out_set);

void oidc_PrepareAuthorization(const oidc_Provider &provider, const char *callback, const char *redirect,
                               Allocator *alloc, oidc_AuthorizationInfo *out_auth);
bool oidc_CheckCookie(Span<const char> cookie, Span<const char> rnd, Allocator *alloc, oidc_CookieInfo *out_info);

bool oidc_ExchangeCode(const oidc_Provider &provider, const char *callback_url, const char *code,
                        Allocator *alloc, oidc_TokenSet *out_set);
bool oidc_DecodeIdToken(const oidc_Provider &provider, Span<const char> token, Span<const char> nonce,
                        Allocator *alloc, oidc_IdentityInfo *out_identity);

}
