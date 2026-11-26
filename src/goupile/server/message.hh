// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"

namespace K {

class InstanceHolder;
struct smtp_Config;
struct smtp_MailContent;
struct sms_Config;

bool IsMailValid(Span<const char> address);
bool IsPhoneValid(Span<const char> number);

bool InitSMTP(const smtp_Config &config);
bool InitSMS(const sms_Config &config);

bool PostMail(const char *to, const smtp_MailContent &content);
bool PostSMS(const char *to, const char *message);

bool SendMails();
bool SendSMS();

void HandleSendMail(http_IO *io, InstanceHolder *instance);
void HandleSendSMS(http_IO *io, InstanceHolder *instance);
void HandleSendTokenize(http_IO *io, InstanceHolder *instance);

}
