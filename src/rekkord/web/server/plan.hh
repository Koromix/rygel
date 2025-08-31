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

namespace K {

void HandlePlanList(http_IO *io);
void HandlePlanGet(http_IO *io);
void HandlePlanSave(http_IO *io);
void HandlePlanDelete(http_IO *io);
void HandlePlanKey(http_IO *io);

void HandlePlanFetch(http_IO *io);
void HandlePlanReport(http_IO *io);

}
