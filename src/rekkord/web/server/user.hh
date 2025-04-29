// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

bool InitSMTP(const smtp_Config &config);

bool PruneTokens();

void HandleUserSession(http_IO *io);
void HandleUserRegister(http_IO *io);
void HandleUserLogin(http_IO *io);
void HandleUserLogout(http_IO *io);
void HandleUserRecover(http_IO *io);
void HandleUserReset(http_IO *io);
void HandleUserPassword(http_IO *io);

void HandlePictureGet(http_IO *io);
void HandlePicturePut(http_IO *io);
void HandlePictureDelete(http_IO *io);

}
