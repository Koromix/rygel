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
    Size Read(Span<uint8_t> out, int timeout);
    Size Read(Span<char> out, int timeout) { return Read(out.As<uint8_t>(), timeout); }
    Size Read(Size len, void *out, int timeout) { return Read(MakeSpan((uint8_t *)out, len), timeout); }

    // Writes are thread safe (mutex)
    bool Write(Span<const uint8_t> buf, int timeout);
    bool Write(Span<const char> buf, int timeout) { return Write(buf.As<const uint8_t>(), timeout); }
    bool Write(char buf, int timeout) { return Write(MakeSpan(&buf, 1), timeout); }
    bool Write(const void *buf, Size len, int timeout) { return Write(MakeSpan((const uint8_t *)buf, len), timeout); }

    bool Ping();

private:
    bool Pump(int64_t deadline);
    bool Send(int opcode, Span<const uint8_t> buf, int64_t deadline);

    void Abort();
};

bool http_IsWS(http_IO *io);

}
