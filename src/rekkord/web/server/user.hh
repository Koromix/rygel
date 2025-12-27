// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"

namespace K {

struct oidc_Provider;
struct smtp_Config;

struct SessionInfo: public RetainObject<SessionInfo> {
    int64_t userid;
    const oidc_Provider *provider;

    // TOTP
    std::atomic_bool totp;
    std::atomic_bool authorized;
    char secret[33];

    // User picture
    std::atomic_int picture;

    char username[];
};

bool PruneTokens();
void PruneSessions();

RetainPtr<SessionInfo> GetNormalSession(http_IO *io);

void HandleUserSession(http_IO *io);
void HandleUserPing(http_IO *io);

void HandleUserRegister(http_IO *io);
void HandleUserLogin(http_IO *io);
void HandleUserLogout(http_IO *io);
void HandleUserRecover(http_IO *io);
void HandleUserReset(http_IO *io);
void HandleUserPassword(http_IO *io);

void HandleSsoLogin(http_IO *io);
void HandleSsoOidc(http_IO *io);
void HandleSsoLink(http_IO *io);

void HandleTotpConfirm(http_IO *io);
void HandleTotpSecret(http_IO *io);
void HandleTotpChange(http_IO *io);
void HandleTotpDisable(http_IO *io);

void HandlePictureGet(http_IO *io);
void HandlePictureSave(http_IO *io);
void HandlePictureDelete(http_IO *io);

}
