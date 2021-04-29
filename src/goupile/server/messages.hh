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

struct MailContent {
    const char *subject = nullptr;
    const char *text = nullptr;
    const char *html = nullptr;
};

bool InitTwilio(const char *sid, const char *token, const char *from);
bool InitSMTP(const char *url, const char *username, const char *password, const char *from);

bool SendSMS(const char *to, const char *message);
bool SendMail(const char *to, const MailContent &content);

}
