// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/http/http.hh"

namespace K {

class InstanceHolder;
struct smtp_MailContent;

bool IsMailValid(Span<const char> address);
bool IsPhoneValid(Span<const char> number);

bool SendMail(const char *to, const smtp_MailContent &content);
bool SendSMS(const char *to, const char *message);

void HandleSendMail(http_IO *io, InstanceHolder *instance);
void HandleSendSMS(http_IO *io, InstanceHolder *instance);
void HandleSendTokenize(http_IO *io, InstanceHolder *instance);

}
