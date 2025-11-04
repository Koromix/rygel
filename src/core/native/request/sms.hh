// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

namespace K {

enum class sms_Provider {
    None,
    Twilio
};
static const char *const sms_ProviderNames[] = {
    "None",
    "Twilio"
};

struct sms_Config {
    sms_Provider provider = sms_Provider::None;
    const char *authid = nullptr;
    const char *token = nullptr;
    const char *from = nullptr;

    bool Validate() const;
};

class sms_Sender {
    sms_Config config;

    BlockAllocator str_alloc;

public:
    bool Init(const sms_Config &config);
    bool Send(const char *to, const char *message);

private:
    bool SendTwilio(const char *to, const char *message);
};

}
