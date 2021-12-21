// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "../../core/libcc/libcc.hh"
#include "http.hh"
#include "../../../vendor/mbedtls/include/mbedtls/sha1.h"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifdef _WIN32
    #include <io.h>
    #include <ws2tcpip.h>
#else
    #include <sys/uio.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <unistd.h>
#endif

namespace RG {

void http_IO::HandleUpgrade(void *cls, struct MHD_Connection *, void *,
                            const char *extra_in, size_t extra_in_size, MHD_socket fd,
                            struct MHD_UpgradeResponseHandle *urh)
{
    http_IO *io = (http_IO *)cls;

    // Notify when handler ends
    RG_DEFER_N(err_guard) {
        MHD_upgrade_action(io->ws_urh, MHD_UPGRADE_ACTION_CLOSE);
        io->ws_cv.notify_one();
    };

    std::lock_guard<std::mutex> lock(io->mutex);

    // Set non-blocking socket behavior
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) < 0) {
        LogError("Failed to make socket non-blocking: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
#else
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        LogError("Failed to make socket non-blocking: %1", strerror(errno));
        return;
    }
    if (!(flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LogError("Failed to make socket non-blocking: %1", strerror(errno));
        return;
    }
#endif

    io->ws_urh = urh;
    io->ws_fd = fd;
    io->ws_buf.Append(MakeSpan((const uint8_t *)extra_in, (Size)extra_in_size));
    io->ws_offset = 0;

#ifdef _WIN32
    io->ws_handle = WSACreateEvent();
    if (!io->ws_handle) {
        LogError("WSACreateEvent() failed: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
    if (WSAEventSelect(fd, io->ws_handle, FD_READ | FD_CLOSE)) {
        LogError("Failed to associate event with socket: %1", GetWin32ErrorString(WSAGetLastError()));
        return;
    }
#endif

    err_guard.Disable();
    io->state = State::WebSocket;
    io->ws_cv.notify_one();
}

bool http_IO::IsWS() const
{
    const char *conn_str = request.GetHeaderValue("Connection");
    const char *upgrade_str = request.GetHeaderValue("Upgrade");

    if (!conn_str || !strstr(conn_str, "Upgrade"))
        return false;
    if (!upgrade_str || !TestStr(upgrade_str, "websocket"))
        return false;

    return true;
}

bool http_IO::UpgradeToWS(unsigned int flags)
{
    RG_ASSERT(state != State::Sync && state != State::WebSocket);
    RG_ASSERT(!force_queue);

    if (!IsWS()) {
        LogError("Missing mandatory WebSocket headers");
        AttachError(400);
        return false;
    }

    // Check WebSocket headers
    const char *key_str;
    {
        const char *version_str = request.GetHeaderValue("Sec-WebSocket-Version");
        key_str = request.GetHeaderValue("Sec-WebSocket-Key");

        if (!version_str || !TestStr(version_str, "13")) {
            LogError("Unsupported Websocket version '%1'", version_str);
            AddHeader("Sec-WebSocket-Version", "13");
            AttachError(426);
            return false;
        }
        if (!key_str) {
            LogError("Missing 'Sec-WebSocket-Key' header");
            AttachError(400);
            return false;
        }
    }

    // Compute accept value. Who designed this?
    char accept_str[128];
    {
        LocalArray<char, 128> full_key;
        uint8_t hash[20];

        full_key.len = Fmt(full_key.data, "%1%2", key_str, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").len;
        mbedtls_sha1((const uint8_t *)full_key.data, full_key.len, hash);
        sodium_bin2base64(accept_str, RG_SIZE(accept_str), hash, RG_SIZE(hash), sodium_base64_VARIANT_ORIGINAL);
    }

    MHD_Response *response = MHD_create_response_for_upgrade(HandleUpgrade, this);
    AttachResponse(101, response);
    AddHeader("Connection", "upgrade");
    AddHeader("Upgrade", "websocket");
    AddHeader("Sec-WebSocket-Accept", accept_str);

    ws_opcode = (flags & (int)http_WebSocketFlag::Text) ? 1 : 2;

    // Wait for the handler to run
    {
        std::unique_lock<std::mutex> lock(mutex);

        force_queue = true;
        while (!ws_urh) {
            if (!daemon->running) {
                LogError("Server is shutting down");
                return false;
            }
            if (state == State::Zombie) {
                LogError("Lost connection during WebSocket upgrade");
                return false;
            }

            Resume();
            ws_cv.wait(lock);
        }
        if (state != State::WebSocket)
            return false;
    }

    return true;
}

void http_IO::OpenForReadWS(StreamReader *out_st)
{
    out_st->Open([this](Span<uint8_t> out_buf) { return ReadWS(out_buf); }, "<ws>");
}

bool http_IO::OpenForWriteWS(CompressionType encoding, StreamWriter *out_st)
{
    bool success = out_st->Open([this](Span<const uint8_t> buf) { return WriteWS(buf); }, "<ws>", encoding);
    return success;
}

Size http_IO::ReadWS(Span<uint8_t> out_buf)
{
#ifndef NDEBUG
    {
        std::unique_lock<std::mutex> lock(mutex);
        RG_ASSERT(state == State::WebSocket || state == State::Zombie);
    }
#endif

    bool begin = false;
    Size read_len = 0;

    while (out_buf.len) {
        // Check status
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (state == State::Zombie)
                break;
        }

        // Decode message
        {
            if (ws_buf.len < 2)
                goto pump;

            int bits = (ws_buf[0] >> 4) & 0xF;
            int opcode = ws_buf[0] & 0xF;
            bool fin = bits & 0xF;

            if (opcode == 1 || opcode == 2) {
                begin = true;
                read_len = 0;
            } else if (opcode == 8) {
                return 0;
            }
            begin &= (opcode < 3);

            bool masked = ws_buf[1] & 0x80;
            Size payload = ws_buf[1] & 0x7F;

            if (bits != 8 && bits != 0) {
                LogError("Unsupported WebSocket RSV bits");
                return -1;
            }
            if (!masked) {
                LogError("Client to server messages must be masked");
                return -1;
            }

            Size offset;
            uint8_t mask[4];
            if (payload == 126) {
                if (ws_buf.len < 8)
                    goto pump;

                uint16_t payload16;
                memcpy_safe(&payload16, ws_buf.ptr + 2, 2);
                memcpy_safe(mask, ws_buf.ptr + 4, 4);

                payload = BigEndian(payload16);
                offset = 8;
            } else if (payload == 127) {
                if (ws_buf.len < 14)
                    goto pump;

                uint64_t payload64;
                memcpy_safe(&payload64, ws_buf.ptr + 2, 8);
                memcpy_safe(mask, ws_buf.ptr + 10, 4);

                if (uint64_t max = Kilobytes(256); payload64 > max) {
                    LogError("Excessive WS packet length %1 (maximum = %2)", FmtMemSize(payload64), FmtMemSize(max));
                    return -1;
                }

                payload = (Size)BigEndian(payload64);
                offset = 14;
            } else {
                if (ws_buf.len < 6)
                    goto pump;

                memcpy_safe(mask, ws_buf.ptr + 2, 4);

                offset = 6;
            }
            if (ws_buf.len - offset < payload)
                goto pump;

            if (begin) {
                Size avail_len = std::min(payload, ws_buf.len - offset);
                Size copy_len = std::min(out_buf.len - read_len, avail_len);

                Size copy4 = copy_len & ~(Size)0x3;
                Size remain = copy_len - copy4;

                for (Size i = 0; i < copy4; i += 4) {
                    out_buf[read_len + 0] = ws_buf[offset + i + 0] ^ mask[0];
                    out_buf[read_len + 1] = ws_buf[offset + i + 1] ^ mask[1];
                    out_buf[read_len + 2] = ws_buf[offset + i + 2] ^ mask[2];
                    out_buf[read_len + 3] = ws_buf[offset + i + 3] ^ mask[3];
                    read_len += 4;
                }
                switch (remain) {
                    default: { RG_UNREACHABLE(); } break;
                    case 3: { out_buf[read_len + 2] = ws_buf[offset + copy4 + 2] ^ mask[2]; } [[fallthrough]];
                    case 2: { out_buf[read_len + 1] = ws_buf[offset + copy4 + 1] ^ mask[1]; } [[fallthrough]];
                    case 1: { out_buf[read_len + 0] = ws_buf[offset + copy4 + 0] ^ mask[0]; } [[fallthrough]];
                    case 0: {} break;
                }
                read_len += remain;
            }

            ws_buf.len = std::max(ws_buf.len - offset - payload, (Size)0);
            memmove_safe(ws_buf.ptr, ws_buf.ptr + offset + payload, ws_buf.len);

            // We can't return empty messages because this is a signal for EOF
            // in the StreamReader code. Oups.
            if (begin && fin && read_len)
                break;
        }

pump:
        // Pump more data from the OS
        ws_buf.Grow(Kibibytes(1));

#ifdef _WIN32
        void *events[2] = {
            ws_handle,
            daemon->stop_handle
        };

        if (WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE) == WSA_WAIT_FAILED) {
            LogError("Failed to read from socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return -1;
        }
        WSAResetEvent(ws_handle);
#else
        struct pollfd pfds[2] = {
            {ws_fd, POLLIN},
            {daemon->stop_pfd[0], POLLIN}
        };

        if (poll(pfds, RG_LEN(pfds), -1) < 0) {
            LogError("Failed to read from socket: %1", strerror(errno));
            return -1;
        }
#endif

        if (RG_UNLIKELY(!daemon->running)) {
            LogError("Server is shutting down");
            return -1;
        }

        ssize_t len = recv(ws_fd, (char *)ws_buf.end(), (int)(ws_buf.capacity - ws_buf.len), 0);
        if (len < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                continue;

#ifdef _WIN32
            LogError("Failed to read from socket: %1", GetWin32ErrorString(WSAGetLastError()));
#else
            LogError("Failed to read from socket: %1", strerror(errno));
#endif
            return -1;
        } else if (!len) {
            break;
        }
        ws_buf.len += (Size)len;
    }

    return read_len;
}

bool http_IO::WriteWS(Span<const uint8_t> buf)
{
#ifndef NDEBUG
    {
        std::unique_lock<std::mutex> lock(mutex);
        RG_ASSERT(state == State::WebSocket || state == State::Zombie);
    }
#endif

    int opcode = ws_opcode;

    while (buf.len) {
        // Check status
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (state == State::Zombie)
                break;
        }

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

#ifdef _WIN32
        if (send(ws_fd, (const char *)frame.data, (int)frame.len, 0) < 0) {
            LogError("Failed to write to socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return false;
        }
        if (send(ws_fd, (const char *)part.ptr, (int)part.len, 0) < 0) {
            LogError("Failed to write to socket: %1", GetWin32ErrorString(WSAGetLastError()));
            return false;
        }
#else
        struct iovec iov[2] = {
            {(void *)frame.data, (size_t)frame.len},
            {(void *)part.ptr, (size_t)part.len}
        };

        if (writev(ws_fd, iov, RG_LEN(iov)) < 0) {
            LogError("Failed to write to socket: %1", strerror(errno));
            return false;
        }
#endif
    }

    return true;
}

}
