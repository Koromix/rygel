// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct smtp_Config;
struct smtp_MailContent;

bool InitSMTP(const smtp_Config &config);

bool PostMail(const char *to, const smtp_MailContent &content);
bool SendMails();

}
