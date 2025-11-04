// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

namespace K {

struct smtp_Config {
    const char *url = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    const char *from = nullptr;

    bool Validate() const;
};

struct smtp_AttachedFile {
    const char *mimetype = nullptr;
    const char *id = nullptr;
    const char *name = nullptr;
    bool inlined = false;
    Span<const uint8_t> data = {};
};

struct smtp_MailContent {
    const char *subject = nullptr;
    const char *text = nullptr;
    const char *html = nullptr;
    Span<const smtp_AttachedFile> files = {};
};

class smtp_Sender {
    smtp_Config config;

    BlockAllocator str_alloc;

public:
    bool Init(const smtp_Config &config);

    const smtp_Config &GetConfig() const { return config; }

    bool Send(const char *to, const smtp_MailContent &content);
    bool Send(const char *to, Span<const char> mail);
};

Span<const char> smtp_BuildMail(const char *from, const char *to, const smtp_MailContent &content, Allocator *alloc);

}
