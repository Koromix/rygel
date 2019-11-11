// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

struct FileEntry {
    const char *url;

    const char *filename;
    FileInfo info;
    char sha256[65];

    RG_HASHTABLE_HANDLER(FileEntry, url);
};

extern HeapArray<FileEntry> app_files;
extern HashTable<const char *, const FileEntry *> app_files_map;

bool InitFiles();

void HandleFileList(const http_RequestInfo &request, http_IO *io);
void HandleFileLoad(const http_RequestInfo &request, const FileEntry &file, http_IO *io);

}
