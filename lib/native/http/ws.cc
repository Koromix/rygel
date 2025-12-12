// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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

#include "lib/native/base/base.hh"
#include "server.hh"
#include "vendor/base64/include/libbase64.h"
#include "vendor/sha1/sha1.h"

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

bool http_IO::IsWS() const
{
    if (!CheckHeaderValue(request.GetHeaderValue("Connection"), "upgrade"))
        return false;
    if (!CheckHeaderValue(request.GetHeaderValue("Upgrade"), "websocket"))
        return false;

    return true;
}

bool http_IO::UpgradeToWS(unsigned int flags)
{
    if (!IsWS()) {
        LogError("Missing mandatory WebSocket headers");
        SendError(400);
        return false;
    }

    // Check WebSocket headers
    const char *key_str;
    {
        const char *version_str = request.GetHeaderValue("Sec-Websocket-Version");
        key_str = request.GetHeaderValue("Sec-Websocket-Key");

        if (!version_str || !TestStr(version_str, "13")) {
            LogError("Unsupported Websocket version '%1'", version_str);
            AddHeader("Sec-WebSocket-Version", "13");
            SendError(426);
            return false;
        }
        if (!key_str) {
            LogError("Missing 'Sec-WebSocket-Key' header");
            SendError(400);
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

    AddHeader("Connection", "upgrade");
    AddHeader("Upgrade", "websocket");
    AddHeader("Sec-WebSocket-Accept", accept_str);
    SendEmpty(101);

    // Corking should be disabled once SendEmpty() returns.
    // And the socket will be in blocking mode, unless I've screwed something up ><

    incoming.buf.len = 0;
    request.keepalive = false;
    ws_opcode = (flags & (int)http_WebSocketFlag::Text) ? 1 : 2;

    return true;
}

void http_IO::OpenForReadWS(StreamReader *out_st)
{
    out_st->Open([this](Span<uint8_t> out_buf) { return ReadWS(out_buf); }, "<ws>");
}

bool http_IO::OpenForWriteWS(StreamWriter *out_st)
{
    bool success = out_st->Open([this](Span<const uint8_t> buf) { return WriteWS(buf); }, "<ws>");
    return success;
}

Size http_IO::ReadWS(Span<uint8_t> out_buf)
{
    bool begin = false;
    Size read_len = 0;

    while (out_buf.len) {
        // Decode message
        {
            if (incoming.buf.len < 2)
                goto pump;

            int bits = (incoming.buf[0] >> 4) & 0xF;
            int opcode = incoming.buf[0] & 0xF;
            bool fin = bits & 0x8;

            if (opcode == 1 || opcode == 2) {
                begin = true;
                read_len = 0;
            } else if (opcode == 8) {
                return 0;
            }
            begin &= (opcode < 3);

            bool masked = incoming.buf[1] & 0x80;
            Size payload = incoming.buf[1] & 0x7F;

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
                if (incoming.buf.len < 8)
                    goto pump;

                uint16_t payload16;
                MemCpy(&payload16, incoming.buf.ptr + 2, 2);
                MemCpy(mask, incoming.buf.ptr + 4, 4);

                payload = BigEndian(payload16);
                offset = 8;
            } else if (payload == 127) {
                if (incoming.buf.len < 14)
                    goto pump;

                uint64_t payload64;
                MemCpy(&payload64, incoming.buf.ptr + 2, 8);
                MemCpy(mask, incoming.buf.ptr + 10, 4);

                payload64 = BigEndian(payload64);

                if (uint64_t max = Mebibytes(4); payload64 > max) {
                    LogError("Excessive WS packet length %1 (maximum = %2)", FmtMemSize(payload64), FmtMemSize(max));
                    return -1;
                }

                payload = (Size)payload64;
                offset = 14;
            } else {
                if (incoming.buf.len < 6)
                    goto pump;

                MemCpy(mask, incoming.buf.ptr + 2, 4);

                offset = 6;
            }
            if (incoming.buf.len - offset < payload)
                goto pump;

            if (begin) {
                Size avail_len = std::min(payload, incoming.buf.len - offset);
                Size copy_len = std::min(out_buf.len - read_len, avail_len);

                Size copy4 = copy_len & ~(Size)0x3;
                Size remain = copy_len - copy4;

                for (Size i = 0; i < copy4; i += 4) {
                    out_buf[read_len + 0] = incoming.buf[offset + i + 0] ^ mask[0];
                    out_buf[read_len + 1] = incoming.buf[offset + i + 1] ^ mask[1];
                    out_buf[read_len + 2] = incoming.buf[offset + i + 2] ^ mask[2];
                    out_buf[read_len + 3] = incoming.buf[offset + i + 3] ^ mask[3];
                    read_len += 4;
                }
                switch (remain) {
                    default: { K_UNREACHABLE(); } break;
                    case 3: { out_buf[read_len + 2] = incoming.buf[offset + copy4 + 2] ^ mask[2]; } [[fallthrough]];
                    case 2: { out_buf[read_len + 1] = incoming.buf[offset + copy4 + 1] ^ mask[1]; } [[fallthrough]];
                    case 1: { out_buf[read_len + 0] = incoming.buf[offset + copy4 + 0] ^ mask[0]; } [[fallthrough]];
                    case 0: {} break;
                }

                read_len += remain;
            }

            incoming.buf.len = std::max(incoming.buf.len - offset - payload, (Size)0);
            MemMove(incoming.buf.ptr, incoming.buf.ptr + offset + payload, incoming.buf.len);

            // We can't return empty messages because this is a signal for EOF
            // in the StreamReader code. Oups.
            if (begin && fin && read_len)
                break;
        }

pump:
        incoming.buf.Grow(Kibibytes(16));

        // Pump more data from the OS/socket
        {
            Size read = daemon->ReadSocket(socket, incoming.buf.TakeAvailable());

            if (read < 0)
                return false;
            if (!read)
                break;

            incoming.buf.len += read;
        }
    }

    return read_len;
}

bool http_IO::WriteWS(Span<const uint8_t> buf)
{
    int opcode = ws_opcode;

    while (buf.len) {
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

        Span<const uint8_t> vec[] = {
            frame,
            part
        };

        if (!daemon->WriteSocket(socket, vec))
            return false;
    }

    return true;
}

}
