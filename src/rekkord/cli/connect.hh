// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "src/rekkord/lib/librekkord.hh"

namespace K {

bool ReportSnapshot(const char *url, const char *key, const char *repository, const char *channel, const rk_SaveInfo &info);
bool ReportError(const char *url, const char *key, const char *repository, const char *channel, int64_t time, const char *message);

}
