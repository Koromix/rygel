// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

class http_Daemon;
class http_IO;
class ws_Socket;

class ws_Registry: public RetainObject<ws_Registry> {
    struct Link {
        std::mutex mutex;
        int sock = -1;
    };

    std::atomic_bool stop { false };
    LockFreePool<Link> links;

public:
    ws_Registry(Size capacity) : links(capacity) {}

    bool Give(int sock, ws_Socket *ws);
    void Release(ws_Socket *ws, bool close);

    void Shutdown();
};

class ws_Socket {
    K_DELETE_COPY(ws_Socket)

    RetainPtr<ws_Registry> registry;
    uint32_t link = 0xFFFFFFFFu;

    int sock = -1;
    std::atomic_bool open { false };
    bool text = false;
    bool eof = false;

    std::mutex mutex;

    LocalArray<uint8_t, 132> buf;

public:
    ws_Socket() = default;
    ws_Socket(http_IO *io, unsigned int flags) { Upgrade(io, flags); }
    ~ws_Socket();

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

    void Release(bool close);

    friend class http_IO;
    friend class ws_Registry;
};

bool http_IsWS(http_IO *io);

}
