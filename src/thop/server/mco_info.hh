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

#include "src/core/base/base.hh"
#include "user.hh"
#include "src/core/network/network.hh"

namespace RG {

void ProduceMcoDiagnoses(const http_RequestInfo &request, const User *user, http_IO *io);
void ProduceMcoProcedures(const http_RequestInfo &request, const User *user, http_IO *io);
void ProduceMcoGhmGhs(const http_RequestInfo &request, const User *user, http_IO *io);

void ProduceMcoTree(const http_RequestInfo &request, const User *user, http_IO *io);
void ProduceMcoHighlight(const http_RequestInfo &request, const User *user, http_IO *io);

}
