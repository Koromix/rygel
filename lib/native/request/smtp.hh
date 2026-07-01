// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct smtp_Config {
    const char *url = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    const char *from = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char>);

    bool Complete();
    bool Validate() const;

    void Clone(smtp_Config *out_config) const;
};

struct smtp_AttachedFile {
    const char *mimetype = nullptr;
    const char *id = nullptr;
    const char *name = nullptr;
    bool inlined = false;
    Span<const uint8_t> data = {};
};

struct smtp_MailContent {
    Span<const char> subject = {};
    Span<const char> text = {};
    Span<const char> html = {};
    Span<const smtp_AttachedFile> files = {};
};

class smtp_Sender {
    smtp_Config config;

public:
    bool Init(const smtp_Config &config);

    bool IsValid() const { return config.url; }
    const smtp_Config &GetConfig() const { return config; }

    bool Send(const char *to, const smtp_MailContent &content);
    bool Send(const char *to, Span<const char> mail);
};

Span<const char> smtp_BuildMail(const char *from, const char *to, const smtp_MailContent &content, Allocator *alloc);

}
