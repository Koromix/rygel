// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

class http_IO;

class http_WS {
    K_DELETE_COPY(http_WS)

    int sock = -1;

    std::atomic_bool open { false };
    bool text = false;

    bool eof = false;
    LocalArray<uint8_t, 132> buf;

    std::mutex mutex;

public:
    http_WS() = default;
    http_WS(http_IO *io, unsigned int flags) { Upgrade(io, flags); }
    ~http_WS();

    bool Upgrade(http_IO *io, unsigned int flags);
    void Close(int status = 1000);

    bool IsValid() const { return open.load(std::memory_order_relaxed); }
    bool IsEOF() const { return eof; }

    // Read from single thread!
    // It will return 0 for empty messages, check EOF status separately.
    // Be careful when using with StreamReader: you should loop and ignore empty messages.
    Size Read(Span<uint8_t> out);
    Size Read(Span<char> out) { return Read(out.As<uint8_t>()); }
    Size Read(Size len, void *out) { return Read(MakeSpan((uint8_t *)out, len)); }

    // Writes are thread safe (mutex)
    bool Write(Span<const uint8_t> buf);
    bool Write(Span<const char> buf) { return Write(buf.As<const uint8_t>()); }
    bool Write(char buf) { return Write(MakeSpan(&buf, 1)); }
    bool Write(const void *buf, Size len) { return Write(MakeSpan((const uint8_t *)buf, len)); }

private:
    bool Pump();
    bool Send(int opcode, Span<const uint8_t> buf);

    void Abort();
};

bool http_IsWS(http_IO *io);

}
