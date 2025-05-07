// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"
#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
#endif
#include "vendor/libssh/include/libssh/libssh.h"
#include "vendor/libssh/include/libssh/sftp.h"

namespace RG {

struct ssh_Config {
    const char *host = nullptr;
    int port = -1;
    const char *username = nullptr;
    const char *path = nullptr;

    bool known_hosts = true;
    const char *fingerprint = nullptr;

    const char *password = nullptr;
    const char *key = nullptr;
    const char *keyfile = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});

    bool Complete();
    bool Validate() const;

    void Clone(ssh_Config *out_config) const;
};

bool ssh_DecodeURL(Span<const char> url, ssh_Config *out_config);

ssh_session ssh_Connect(const ssh_Config &config);

const char *sftp_GetErrorString(sftp_session sftp);

}
