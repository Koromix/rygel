// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

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
