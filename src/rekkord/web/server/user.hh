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

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace RG {

struct smtp_Config;

struct SessionInfo: public RetainObject<SessionInfo> {
    int64_t userid;

    std::atomic_bool totp;
    std::atomic_bool confirmed;
    char secret[33];

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

void HandleTotpConfirm(http_IO *io);
void HandleTotpSecret(http_IO *io);
void HandleTotpChange(http_IO *io);
void HandleTotpDisable(http_IO *io);

void HandlePictureGet(http_IO *io);
void HandlePictureSave(http_IO *io);
void HandlePictureDelete(http_IO *io);

}
