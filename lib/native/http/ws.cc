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

static Size ReadSocket(int sock, Span<uint8_t> buf)
{
    int len = (int)std::min(buf.len, (Size)INT_MAX);
    Size bytes = recv((SOCKET)sock, (char *)buf.ptr, len, 0);

    if (bytes < 0) {
        int error = GetLastError();

        if (error != WSAENOTCONN && error != WSAECONNRESET) {
            LogError("Failed to read from client: %1", GetWin32ErrorString(error));
        }

        return -1;
    }

    return bytes;
}

static bool WriteSocket(int sock, Span<const uint8_t> header, Span<const uint8_t> data)
{
    K_ASSERT(header.len);

    WSABUF parts[2] = {
        { .len = (unsigned long)header.len, .buf = (char *)header.ptr },
        { .len = (unsigned long)data.len, .buf = (char *)data.ptr }
    };

    DWORD sent = 0;
    int ret = WSASend((SOCKET)sock, parts, data.len ? 2 : 1, &sent, 0, nullptr, nullptr);

    if (ret) {
        int error = GetLastError();

        if (error != WSAENOTCONN && error != WSAECONNRESET) {
            LogError("Failed to send to client: %1", GetWin32ErrorString(error));
        }

        return false;
    }

    // Windows does not apparently do partial writes, so don't bother dealing with that.
    //
    // From MSDN:
    // For non-overlapped sockets [...] WSASend adopts the same blocking semantics as send.
    // [...] Given the same buffer situation and a blocking socket, WSASend will block until
    // all of the application's buffer contents have been consumed.

    return true;
}

#else

static Size ReadSocket(int sock, Span<uint8_t> buf)
{
restart:
    Size bytes = recv(sock, buf.ptr, (size_t)buf.len, 0);

    if (bytes < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to read from WebSocket: %1", strerror(errno));
        }

        return -1;
    }

    return bytes;
}

static bool WriteSocket(int sock, Span<const uint8_t> header, Span<const uint8_t> data)
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

    while (msg.msg_iovlen) {
        Size sent = sendmsg(sock, &msg, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

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

bool http_WS::Upgrade(http_IO *io, unsigned int flags)
{
    K_ASSERT(sock < 0);

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

    sock = io->LeakSocket();
    if (sock < 0)
        return false;

    io->AddHeader("Connection", "upgrade");
    io->AddHeader("Upgrade", "websocket");
    io->AddHeader("Sec-Websocket-Accept", accept_str);

    if (!io->SendEmpty(101)) {
        Close(-1);
        return false;
    }

    // Corking should be disabled once SendEmpty() returns.
    // And the socket will be in blocking mode, unless I've screwed something up ><

    text = flags & (int)http_WebSocketFlag::Text;
    eof = false;

    return true;
}

void http_WS::Close(int status)
{
    if (sock < 0)
        return;

    if (status < 0) {
        struct linger sl = { 1, 0 };
#if defined(_WIN32)
        setsockopt((SOCKET)sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
#else
        setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
#endif
    } else {
        // Best effort! May fail, completely or partially, let the kernel handle the rest.
        SetSocketNonBlock(sock, true);

        if (status) {
            uint8_t payload[2] = { (uint8_t)(status >> 8), (uint8_t)(status & 0xFF) };
            Send(8, payload);
        } else {
            Send(8, {});
        }
    };

    CloseSocket(sock);
    sock = -1;

    buf.Clear();
}

static void Decode(const uint8_t *src, uint8_t mask[4], Size len, uint8_t *dest)
{
    Size len4 = len & ~(Size)0x3;
    Size remain = len - len4;

    for (Size i = 0; i < len4; i += 4) {
        dest[0] = src[0] ^ mask[0];
        dest[1] = src[1] ^ mask[1];
        dest[2] = src[2] ^ mask[2];
        dest[3] = src[3] ^ mask[3];

        src += 4;
        dest += 4;
    }

    switch (remain) {
        case 3: { dest[2] = src[2] ^ mask[2]; } [[fallthrough]];
        case 2: { dest[1] = src[1] ^ mask[1]; } [[fallthrough]];
        case 1: { dest[0] = src[0] ^ mask[0]; } [[fallthrough]];
        case 0: {} break;
    }
}

Size http_WS::Read(Span<uint8_t> out_buf)
{
    bool begin = false;
    Size read_len = 0;

    for (;;) {
        // Decode message
        {
            if (buf.len < 2)
                goto pump;

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

                if (buf.len < 6)
                    goto pump;

                uint8_t mask[4];
                MemCpy(mask, buf.ptr + 2, 4);

                if (buf.len - 6 < payload)
                    goto pump;

                if (payload) {
                    K_ASSERT(payload >= 2);

                    int status = ((int)(buf[6] ^ mask[0]) << 8) | (buf[7] ^ mask[1]);
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

                if (buf.len < 6)
                    goto pump;

                uint8_t mask[4];
                MemCpy(mask, buf.ptr + 2, 4);

                if (buf.len - 6 < payload)
                    goto pump;

                if (opcode == 9) {
                    LocalArray<uint8_t, 128> pong;

                    pong.len = payload;
                    Decode(buf.ptr + 6, mask, payload, pong.data);

                    if (!Send(10, pong))
                        return -1;
                }

                MemMove(buf.ptr, buf.ptr + 6 + payload, buf.len - 6 - payload);
                buf.len -= 6 + payload;

                continue;
            } else if (opcode < 3) { // Data (or whatever)
                Size offset = 0;
                uint8_t mask[4];

                if (payload == 126) {
                    if (buf.len < 8)
                        goto pump;

                    uint16_t payload16;
                    MemCpy(&payload16, buf.ptr + 2, 2);
                    MemCpy(mask, buf.ptr + 4, 4);

                    payload = BigEndian(payload16);
                    offset = 8;
                } else if (payload == 127) {
                    if (buf.len < 14)
                        goto pump;

                    uint64_t payload64;
                    MemCpy(&payload64, buf.ptr + 2, 8);
                    MemCpy(mask, buf.ptr + 10, 4);

                    payload64 = BigEndian(payload64);

                    if (uint64_t max = Mebibytes(4); payload64 > max) {
                        LogError("Excessive WS packet length %1 (maximum = %2)", FmtMemSize(payload64), FmtMemSize(max));
                        Close(1002);
                        return -1;
                    }

                    payload = (Size)payload64;
                    offset = 14;
                } else {
                    if (buf.len < 6)
                        goto pump;

                    MemCpy(mask, buf.ptr + 2, 4);

                    offset = 6;
                }
                if (buf.len - offset < payload)
                    goto pump;

                if (opcode) {
                    if (begin) {
                        LogError("Unfinished message in WebSocket stream");
                        Close(1002);
                        return -1;
                    }

                    begin = true;
                    read_len = 0;
                } else if (!begin) {
                    LogError("Unexpected continuation frame in WebSocket stream");
                    Close(1002);
                    return -1;
                }

                // Decode payload
                {
                    Size avail_len = std::min(payload, buf.len - offset);
                    Size copy_len = std::min(out_buf.len - read_len, avail_len);

                    Decode(buf.ptr + offset, mask, copy_len, out_buf.ptr + read_len);
                    read_len += copy_len;
                }

                buf.len = std::max(buf.len - offset - payload, (Size)0);
                MemMove(buf.ptr, buf.ptr + offset + payload, buf.len);

                if (fin)
                    break;
            } else {
                LogError("Unsupported WebSocket opcode");
                Close(1002);
                return -1;
            }
        }

pump:
        buf.Grow(Kibibytes(16));

        // Pump more data from the OS/socket
        {
            Size read = ReadSocket(sock, buf.TakeAvailable());

            if (read < 0)
                return -1;
            if (!read) {
                eof = true;
                break;
            }

            buf.len += read;
        }
    }

    return read_len;
}

bool http_WS::Write(Span<const uint8_t> buf)
{
    int opcode = text ? 1 : 2;
    return Send(opcode, buf);
}

bool http_WS::Send(int opcode, Span<const uint8_t> buf)
{
    std::lock_guard<std::mutex> lock(mutex);

    do {
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

        if (!WriteSocket(sock, frame, part))
            return false;
    } while (buf.len);

    return true;
}

}
