// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

// External code can use this type as a handle, see LockFile
struct FileEntry;

bool InitFiles();

void HandleFileList(const http_RequestInfo &request, http_IO *io);

const FileEntry *LockFile(const char *url);
void UnlockFile(const FileEntry *file);

// File needs to be locked (see LockFile and UnlockFile)
void HandleFileGet(const http_RequestInfo &request, const FileEntry &file, http_IO *io);

void HandleFilePut(const http_RequestInfo &request, http_IO *io);
void HandleFileDelete(const http_RequestInfo &request, http_IO *io);

}
