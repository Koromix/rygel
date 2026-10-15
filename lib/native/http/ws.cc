// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "server.hh"
#include "misc.hh"
#include "ws.hh"
#include "vendor/base64/include/libbase64.h"
#include "vendor/sha1/sha1.h"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <ws2tcpip.h>
    #include <mswsock.h>
#else
    #include <sys/socket.h>
    #include <poll.h>
#endif

#if defined(__APPLE__)
    // The flag may exist, but it does not seem to work with recv!
    #undef MSG_DONTWAIT
#endif

namespace K {

static bool CheckHeaderValue(const char *str, const char *needle)
{
    if (!str)
        return false;

    while (str[0]) {
        Span<const char> part = TrimStr(SplitStr(str, ',', &str));

        if (TestStrI(part, needle))
            return true;
    }

    return false;
}

#if defined(_WIN32)

static Size ReadSocket(int sock, Span<uint8_t> buf, int64_t deadline)
{
restart:
    int len = (int)std::min(buf.len, (Size)INT_MAX);
    Size bytes = recv((SOCKET)sock, (char *)buf.ptr, len, 0);

    if (bytes < 0) {
        int error = GetLastError();

        if (error == WSAEWOULDBLOCK) {
            WSAPOLLFD pfd = { (SOCKET)sock, POLLIN, 0 };

            int timeout = (int)std::max((int64_t)0, deadline - GetMonotonicClock());
            int ready = WSAPoll(&pfd, 1, timeout);

            if (ready > 0)
                goto restart;
        }

        if (error != WSAENOTCONN && error != WSAECONNRESET) {
            LogError("Failed to read from client: %1", GetWin32ErrorString(error));
        }

        return -1;
    }

    return bytes;
}

static bool WriteSocket(int sock, Span<const uint8_t> header, Span<const uint8_t> data, int64_t deadline)
{
    K_ASSERT(header.len);

    WSABUF parts[2] = {
        { .len = (unsigned long)header.len, .buf = (char *)header.ptr },
        { .len = (unsigned long)data.len, .buf = (char *)data.ptr }
    };
    Span<WSABUF> buffers = MakeSpan(parts, data.len ? 2 : 1);

    while (buffers.len) {
        DWORD sent = 0;
        int ret = WSASend((SOCKET)sock, buffers.ptr, buffers.len, &sent, 0, nullptr, nullptr);

        if (ret) {
            int error = GetLastError();

            if (error == WSAEWOULDBLOCK) {
                WSAPOLLFD pfd = { (SOCKET)sock, POLLOUT, 0 };

                int timeout = (int)std::max((int64_t)0, deadline - GetMonotonicClock());
                int ready = WSAPoll(&pfd, 1, timeout);

                if (ready > 0)
                    continue;
            }

            if (error != WSAENOTCONN && error != WSAECONNRESET) {
                LogError("Failed to send to client: %1", GetWin32ErrorString(error));
            }

            return false;
        }

        do {
            WSABUF *part = buffers.ptr;

            if (part->len > (unsigned long)sent) {
                part->buf = (char *)part->buf + sent;
                part->len -= (unsigned long)sent;

                break;
            }

            buffers.ptr++;
            buffers.len--;
            sent -= (DWORD)part->len;
        } while (buffers.len);
    }

    return true;
}

#else

static Size ReadSocket(int sock, Span<uint8_t> buf, int64_t deadline)
{
#if defined(MSG_DONTWAIT)
    int flags = MSG_DONTWAIT;
#else
    int flags = 0;
#endif

restart:
    Size bytes = recv(sock, buf.ptr, (size_t)buf.len, flags);

    if (bytes < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pfd = { sock, POLLIN, 0 };

            int timeout = (int)std::max((int64_t)0, deadline - GetMonotonicClock());
            int ready = poll(&pfd, 1, timeout);

            if (ready > 0)
                goto restart;
            if (ready < 0 && errno == EINTR)
                goto restart;
        }

        if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to read from WebSocket: %1", strerror(errno));
        }

        return -1;
    }

    return bytes;
}

static bool WriteSocket(int sock, Span<const uint8_t> header, Span<const uint8_t> data, int64_t deadline)
{
    K_ASSERT(header.len);

    struct iovec parts[2] = {
        { .iov_base = (char *)header.ptr, .iov_len = (size_t)header.len },
        { .iov_base = (char *)data.ptr, .iov_len = (size_t)data.len },
    };
    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = parts,
        .msg_iovlen = (decltype(msghdr::msg_iovlen))(data.len ? 2 : 1),
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };

#if defined(MSG_DONTWAIT)
    int flags = MSG_NOSIGNAL | MSG_DONTWAIT;
#else
    int flags = MSG_NOSIGNAL;
#endif

    while (msg.msg_iovlen) {
        Size sent = sendmsg(sock, &msg, flags);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = { sock, POLLOUT, 0 };

                int timeout = (int)std::max((int64_t)0, deadline - GetMonotonicClock());
                int ready = poll(&pfd, 1, timeout);

                if (ready > 0)
                    continue;
                if (ready < 0 && errno == EINTR)
                    continue;
            }

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            return false;
        }

        do {
            struct iovec *part = msg.msg_iov;

            if (part->iov_len > (size_t)sent) {
                part->iov_base = (uint8_t *)part->iov_base + sent;
                part->iov_len -= (size_t)sent;

                break;
            }

            msg.msg_iov++;
            msg.msg_iovlen--;
            sent -= (Size)part->iov_len;
        } while (msg.msg_iovlen);

        if (!msg.msg_iovlen)
            break;
    }

    return true;
}

#endif

bool http_IsWS(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    if (request.method != http_RequestMethod::Get || request.head || request.version < 11)
        return false;

    if (!CheckHeaderValue(request.GetHeaderValue("Connection"), "upgrade"))
        return false;
    if (!CheckHeaderValue(request.GetHeaderValue("Upgrade"), "websocket"))
        return false;

    return true;
}

static bool IsKeyValid(Span<const char> str)
{
    if (str.len != 24)
        return false;
    if (!std::all_of(str.ptr, str.ptr + 22, [](int c) { return IsAsciiAlphaOrDigit(c) || c == '+' || c == '/'; }))
        return false;
    if (str[22] != '=' || str[23] != '=')
        return false;

    return true;
}

http_WS::~http_WS()
{
    if (IsValid()) {
        LogDebug("Implicit close of WebSocket 0x%1 (%2)", this, sock);
        Abort();
    }

    CloseSocket(sock);
}

bool http_WS::Upgrade(http_IO *io, unsigned int flags)
{
    K_ASSERT(!open);

    const http_RequestInfo &request = io->Request();

    // Safety precautions
    if (!http_IsWS(io)) {
        LogError("Missing mandatory WebSocket headers");
        io->SendError(400);
        return false;
    }
    if (!http_PreventCSRF(io))
        return false;

    // Check WebSocket headers
    Span<const char> key_str;
    {
        const char *version_str = request.GetHeaderValue("Sec-Websocket-Version");
        key_str = request.GetHeaderValue("Sec-Websocket-Key");

        if (!version_str || !TestStr(version_str, "13")) {
            LogError("Unsupported Websocket version '%1'", version_str);
            io->AddHeader("Sec-Websocket-Version", "13");
            io->SendError(426);
            return false;
        }
        if (!IsKeyValid(key_str)) {
            LogError("Missing or invalid 'Sec-WebSocket-Key' header");
            io->SendError(400);
            return false;
        }
    }

    // Compute accept value
    char accept_str[128];
    {
        LocalArray<char, 128> full_key;
        uint8_t hash[20];

        full_key.len = Fmt(full_key.data, "%1%2", key_str, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").len;
        SHA1((unsigned char *)hash, (const unsigned char *)full_key.data, (size_t)full_key.len);

        size_t len = 0;
        base64_encode((const char *)hash, K_SIZE(hash), accept_str, &len, 0);
        accept_str[len] = 0;
    }

    if (sock >= 0) {
        Abort();
        CloseSocket(sock);
    }

    sock = io->LeakSocket();
    if (sock < 0)
        return false;

    io->AddHeader("Connection", "upgrade");
    io->AddHeader("Upgrade", "websocket");
    io->AddHeader("Sec-Websocket-Accept", accept_str);

    if (!io->SendEmpty(101)) {
        Abort();

        CloseSocket(sock);
        sock = -1;

        return false;
    }

#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(sock, true);
#endif

    // Corking should be disabled once SendEmpty() returns.
    // And the socket will be in blocking mode, unless I've screwed something up ><

    open.store(true, std::memory_order_relaxed);
    text = flags & (int)http_WebSocketFlag::Text;
    eof = false;
    buf.Clear();

    return true;
}

void http_WS::Close(int status)
{
    K_ASSERT(status >= 0);

    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    if (!lock.owns_lock()) {
        Abort();
        return;
    }
    if (!IsValid())
        return;

    if (status) {
        uint8_t payload[2] = { (uint8_t)(status >> 8), (uint8_t)(status & 0xFF) };
        Send(8, payload, 0);
    } else {
        Send(8, {}, 0);
    }

#if defined(_WIN32)
    shutdown((SOCKET)sock, SD_SEND);
#else
    shutdown(sock, SHUT_WR);
#endif

    open.store(false, std::memory_order_relaxed);
}

static void Decode(Span<uint8_t> buf, uint8_t mask[4])
{
    uint8_t *ptr = buf.ptr;
    Size len4 = buf.len & ~(Size)0x3;
    Size remain = buf.len - len4;

    for (Size i = 0; i < len4; i += 4) {
        ptr[0] ^= mask[0];
        ptr[1] ^= mask[1];
        ptr[2] ^= mask[2];
        ptr[3] ^= mask[3];
        ptr += 4;
    }

    switch (remain) {
        case 3: { ptr[2] ^= mask[2]; } [[fallthrough]];
        case 2: { ptr[1] ^= mask[1]; } [[fallthrough]];
        case 1: { ptr[0] ^= mask[0]; } [[fallthrough]];
        case 0: {} break;
    }
}

static bool IsCloseStatusValid(int status)
{
    if (status >= 1000 && status <= 1003)
        return true;
    if (status >= 1007 && status <= 1014)
        return true;
    if (status >= 3000 && status <= 4999)
        return true;

    return false;
}

Size http_WS::Read(Span<uint8_t> out, int timeout)
{
    K_ASSERT(timeout >= 0);

    if (eof)
        return 0;
    if (!IsValid())
        return -1;

    int64_t deadline = GetMonotonicClock() + timeout;

    bool begin = false;
    Size message = 0;

    for (;;) {
        if (buf.len < 2) {
            if (!Pump(deadline))
                return -1;
            continue;
        }

        int bits = (buf[0] >> 4) & 0xF;
        int opcode = buf[0] & 0xF;
        bool fin = bits & 0x8;

        bool masked = buf[1] & 0x80;
        Size payload = buf[1] & 0x7F;

        if (bits != 8 && bits != 0) {
            LogError("Unsupported WebSocket RSV bits");
            Close(1002);
            return -1;
        }
        if (!masked) {
            LogError("Client to server messages must be masked");
            Close(1002);
            return -1;
        }

        if (opcode == 8) { // Close
            if (!fin || payload == 1 || payload > 125) {
                LogError("Invalid close message");
                Close(1002);
                return -1;
            }

            if (buf.len < 6) {
                if (!Pump(deadline))
                    return -1;
                continue;
            }

            uint8_t mask[4];
            MemCpy(mask, buf.data + 2, 4);

            if (buf.len - 6 < payload) {
                if (!Pump(deadline))
                    return -1;
                continue;
            }

            if (payload) {
                K_ASSERT(payload >= 2);

                int status = ((int)(buf[6] ^ mask[0]) << 8) | (buf[7] ^ mask[1]);

                status = IsCloseStatusValid(status) ? status : 1002;
                Close(status);
            } else {
                Close(0);
            }

            eof = true;
            return 0;
        } else if (opcode == 9 || opcode == 10) { // Ping or Pong
            if (!fin || payload > 125) {
                LogError("Invalid ping message");
                Close(1002);
                return -1;
            }

            if (buf.len < 6) {
                if (!Pump(deadline))
                    return -1;
                continue;
            }

            uint8_t mask[4];
            MemCpy(mask, buf.data + 2, 4);

            if (buf.len - 6 < payload) {
                if (!Pump(deadline))
                    return -1;
                continue;
            }

            if (opcode == 9) {
                std::lock_guard<std::mutex> lock(mutex);

                Span<uint8_t> pong = buf.Take(6, payload);
                Decode(pong, mask);

                if (!Send(10, pong, 0))
                    return -1;
            }

            MemMove(buf.data, buf.data + 6 + payload, buf.len - 6 - payload);
            buf.len -= 6 + payload;
        } else if (opcode < 3) { // Data (or whatever)
            Size offset = 0;
            uint8_t mask[4];

            if (payload == 126) {
                if (buf.len < 8) {
                    if (!Pump(deadline))
                        return -1;
                    continue;
                }

                uint16_t payload16;
                MemCpy(&payload16, buf.data + 2, 2);
                MemCpy(mask, buf.data + 4, 4);

                payload = BigEndian(payload16);
                offset = 8;
            } else if (payload == 127) {
                if (buf.len < 14) {
                    if (!Pump(deadline))
                        return -1;
                    continue;
                }

                uint64_t payload64;
                MemCpy(&payload64, buf.data + 2, 8);
                MemCpy(mask, buf.data + 10, 4);

                payload64 = BigEndian(payload64);

                if (payload64 > K_SIZE_MAX) {
                    LogError("Invalid WebSocket message length");
                    Close(1002);
                    return -1;
                }

                payload = (Size)payload64;
                offset = 14;
            } else {
                if (buf.len < 6) {
                    if (!Pump(deadline))
                        return -1;
                    continue;
                }

                MemCpy(mask, buf.data + 2, 4);

                offset = 6;
            }

            if (opcode) {
                if (begin) {
                    LogError("Unfinished message in WebSocket stream");
                    Close(1002);
                    return -1;
                }

                begin = true;
            } else if (!begin) {
                LogError("Unexpected continuation frame in WebSocket stream");
                Close(1002);
                return -1;
            }
            if (payload > out.len) {
                LogError("Excessive WebSocket message length");
                Close(1009);
                return -1;
            }

            Size end = std::min(buf.len, offset + payload);
            Size available = end - offset;

            // Copy previously fetched data
            MemCpy(out.ptr, buf.data + offset, available);
            MemMove(buf.data, buf.data + end, buf.len - end);
            buf.len -= end;

            // Pump missing data (if any)
            while (available < payload) {
                Span<uint8_t> span = out.Take(available, payload - available);
                Size read = ReadSocket(sock, span, deadline);

                if (read < 0) {
                    Abort();
                    return -1;
                }
                if (!read) {
                    Close(1002);
                    return -1;
                }

                available += read;
            }

            Decode(out.Take(0, payload), mask);
            message += payload;

            if (fin)
                return message;

            out.ptr += payload;
            out.len -= payload;
        } else {
            LogError("Unsupported WebSocket opcode");
            Close(1002);
            return -1;
        }
    }
}

bool http_WS::Write(Span<const uint8_t> buf, int timeout)
{
    K_ASSERT(timeout >= 0);

    std::lock_guard<std::mutex> lock(mutex);

    if (!IsValid())
        return false;

    int opcode = text ? 1 : 2;
    int64_t deadline = GetMonotonicClock() + timeout;

    return Send(opcode, buf, deadline);
}

bool http_WS::Ping()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!IsValid())
        return false;

    return Send(9, {}, 0);
}

bool http_WS::Pump(int64_t deadline)
{
    K_ASSERT(buf.Available());

    Size read = ReadSocket(sock, buf.TakeAvailable(), deadline);

    if (read < 0) {
        Abort();
        return false;
    }
    if (!read) {
        Close(1002);
        return false;
    }

    buf.len += read;

    return true;
}

bool http_WS::Send(int opcode, Span<const uint8_t> buf, int64_t deadline)
{
    for (;;) {
        Size part_len = std::min(buf.len, (Size)4096 - 4);
        Span<const uint8_t> part = buf.Take(0, part_len);

        buf = buf.Take(part_len, buf.len - part_len);

        LocalArray<uint8_t, 4> frame;

        frame.data[0] = (uint8_t)((buf.len ? 0 : 0x8 << 4) | opcode);
        if (part_len >= 126) {
            frame.data[1] = 126;
            frame.data[2] = (uint8_t)(part_len >> 8);
            frame.data[3] = (uint8_t)(part_len & 0xFF);
            frame.len = 4;
        } else {
            frame.data[1] = (uint8_t)part_len;
            frame.len = 2;
        }
        opcode = 0;

        if (!WriteSocket(sock, frame, part, deadline)) {
            Abort();
            return false;
        }

        if (!buf.len)
            break;
    }

    return true;
}

void http_WS::Abort()
{
#if defined(_WIN32)
    struct linger sl = { 1, 0 };
    setsockopt((SOCKET)sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
    shutdown((SOCKET)sock, SD_BOTH);
#else
    struct linger sl = { 1, 0 };
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
    shutdown(sock, SHUT_RDWR);
#endif

    open.store(false, std::memory_order_relaxed);
}

}
