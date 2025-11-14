// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "vendor/libssh/include/libssh/libssh.h"
#include "vendor/libssh/include/libssh/sftp.h"

namespace K {

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
const char *ssh_MakeURL(const ssh_Config &config, Allocator *alloc);

ssh_session ssh_Connect(const ssh_Config &config);

const char *sftp_GetErrorString(sftp_session sftp);

}
