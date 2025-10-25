// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
