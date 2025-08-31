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
