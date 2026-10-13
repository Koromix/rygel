// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

class http_IO;

class http_WS {
    K_DELETE_COPY(http_WS)

    int sock = -1;
    bool text = false;

    bool eof = false;
    HeapArray<uint8_t> buf;

    std::mutex mutex;

public:
    http_WS() = default;
    http_WS(http_IO *io, unsigned int flags) { Upgrade(io, flags); }
    ~http_WS() { Close(-1); }

    bool Upgrade(http_IO *io, unsigned int flags);
    void Close() { Close(1000); };

    bool IsValid() const { return sock >= 0; }
    bool IsEOF() const { return eof; }

    // Read from single thread!
    Size Read(Span<uint8_t> out_buf);
    Size Read(Span<char> out_buf) { return Read(out_buf.As<uint8_t>()); }
    Size Read(Size buf_len, void *out_buf) { return Read(MakeSpan((uint8_t *)out_buf, buf_len)); }

    // Writes are thread safe (mutex)
    bool Write(Span<const uint8_t> buf);
    bool Write(Span<const char> buf) { return Write(buf.As<const uint8_t>()); }
    bool Write(char buf) { return Write(MakeSpan(&buf, 1)); }
    bool Write(const void *buf, Size len) { return Write(MakeSpan((const uint8_t *)buf, len)); }

private:
    bool Send(int opcode, Span<const uint8_t> buf);

    void Close(int status);
};

bool http_IsWS(http_IO *io);

}
