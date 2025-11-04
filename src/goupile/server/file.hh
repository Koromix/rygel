// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/http/http.hh"

namespace K {

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
    K_ASSERT(hash.len == 32);
    Fmt(MakeSpan(out_sha256, 65), "%1", FmtHex(hash));
}

}
