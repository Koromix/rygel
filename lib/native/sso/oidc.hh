// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

enum class oidc_JwtSigningAlgorithm {
    RS256,
    PS256,
    HS256
};
static const char *const oidc_JwtSigningAlgorithmNames[] = {
    "RS256",
    "PS256",
    "HS256"
};

struct oidc_Provider {
    const char *key = nullptr;
    const char *title = nullptr;

    const char *discovery_url = nullptr;
    const char *client_id = nullptr;
    const char *client_secret = nullptr;
    oidc_JwtSigningAlgorithm jwt_algorithm = oidc_JwtSigningAlgorithm::RS256;
    bool explicit_algorithm = false;
    bool tolerate_unknown_audiences = false;
    bool explicit_tolerate = false;

    // Auto-discovery
    const char *issuer = nullptr;
    const char *auth_url = nullptr;
    const char *token_url = nullptr;
    const char *logout_url = nullptr;
    const char *jwks_url = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory);
    bool Discover();

    bool Complete();
    bool Validate(bool partial = false) const;
};

struct oidc_AuthorizationInfo {
    const char *url = nullptr;
    const char *cookie = nullptr;
};

struct oidc_CookieInfo {
    const char *key = nullptr;
    const char *redirect = nullptr;
    Span<const char> nonce = {};
};

struct oidc_TokenSet {
    const char *id = nullptr;
    const char *access = nullptr; // Can be NULL
};

struct oidc_IdentityInfo {
    const char *sub = nullptr;
    const char *sid = nullptr;

    const char *email = nullptr;
    bool email_verified = false;

    const oidc_Provider *provider = nullptr;
};

struct oidc_LogoutInfo {
    const char *sub = nullptr;
    const char *sid = nullptr;
};

void oidc_PrepareAuthorization(const oidc_Provider &provider, const char *scopes,
                               const char *callback, const char *redirect, const char *claims,
                               Allocator *alloc, oidc_AuthorizationInfo *out_auth);

bool oidc_CheckCookie(Span<const char> cookie, Span<const char> rnd, Allocator *alloc, oidc_CookieInfo *out_info);
bool oidc_ExchangeCode(const oidc_Provider &provider, const char *callback_url, const char *code,
                        Allocator *alloc, oidc_TokenSet *out_set);
bool oidc_DecodeIdToken(const oidc_Provider &provider, Span<const char> token, Span<const char> nonce,
                        Allocator *alloc, oidc_IdentityInfo *out_identity);

bool oidc_DecodeLogoutToken(const oidc_Provider &provider, Span<const char> token, Allocator *alloc, oidc_LogoutInfo *out_logout);
const char *oidc_FormatLogoutUrl(const oidc_Provider &provider, const char *redirect, Allocator *alloc);

}
