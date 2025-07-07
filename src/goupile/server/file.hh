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

namespace RG {

class InstanceHolder;

bool ServeFile(http_IO *io, InstanceHolder *instance, const char *sha256, const char *filename, bool download, int64_t max_age);
bool PutFile(http_IO *io, InstanceHolder *instance, CompressionType compression_type, const char *expect, const char **out_sha256);

void HandleFileList(http_IO *io, InstanceHolder *instance);
bool HandleFileGet(http_IO *io, InstanceHolder *instance);
void HandleFilePut(http_IO *io, InstanceHolder *instance);
void HandleFileDelete(http_IO *io, InstanceHolder *instance);
void HandleFileHistory(http_IO *io, InstanceHolder *instance);
void HandleFileRestore(http_IO *io, InstanceHolder *instance);
void HandleFileDelta(http_IO *io, InstanceHolder *instance);
void HandleFilePublish(http_IO *io, InstanceHolder *instance);

static inline void FormatSha256(Span<const uint8_t> hash, char out_sha256[65])
{
    RG_ASSERT(hash.len == 32);
    Fmt(MakeSpan(out_sha256, 65), "%1", FmtSpan(hash, FmtType::BigHex, "").Pad0(-2));
}

}
