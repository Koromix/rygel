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

#if !defined(_WIN32)

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"
#include "posix_priv.hh"

#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace RG {

// Sane platform
static_assert(EAGAIN == EWOULDBLOCK);

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
restart:
    Size bytes = recv(socket->sock, buf.ptr, (size_t)buf.len, 0);

    if (bytes < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to read from client: %1", strerror(errno));
        }
        return -1;
    }

    socket->client.timeout_at = GetMonotonicTime() + idle_timeout;

    return bytes;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf)
{
    int flags = MSG_NOSIGNAL;

#if defined(MSG_MORE)
    flags |= MSG_MORE;
#endif

    while (buf.len) {
        Size len = std::min(buf.len, Mebibytes(2));
        Size bytes = send(socket->sock, buf.ptr, len, flags);

        if (bytes < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        socket->client.timeout_at = GetMonotonicTime() + send_timeout;

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    if (!data.len) {
        bool success = (response.expected < 0 || response.sent == response.expected);

        uint8_t end[5] = { '0', '\r', '\n', '\r', '\n' };
        success &= daemon->WriteSocket(socket, end);

        SetDescriptorRetain(socket->sock, false);

        request.keepalive &= success;
        return success;
    }

    if (data.len > (Size)16 * 0xFFFF) [[unlikely]] {
        do {
            Size take = std::min((Size)16 * 0xFFFF, data.len);
            Span<const uint8_t> frag = data.Take(0, take);

            if (!WriteChunked(frag))
                return false;

            data.ptr += take;
            data.len -= take;
        } while (data.len);

        return true;
    }

    char full[6] = { 'F', 'F', 'F', 'F', '\r', '\n' };
    char last[6] = { 0, 0, 0, 0, '\r', '\n' };
    char tail[2] = { '\r', '\n' };

    LocalArray<struct iovec, 3 * 16> parts;
    Size remain = 0;

    while (data.len >= 0xFFFF) {
        remain += 0xFFFF + RG_SIZE(full) + RG_SIZE(tail);

        parts.Append({ full, RG_SIZE(full) });
        parts.Append({ (void *)data.ptr, 0xFFFF });
        parts.Append({ tail, RG_SIZE(tail) });

        data.ptr += 0xFFFF;
        data.len -= 0xFFFF;
    }

    if (data.len) {
        static const char literals[] = "0123456789ABCDEF";

        last[0] = literals[((size_t)data.len >> 12) & 0xF];
        last[1] = literals[((size_t)data.len >> 8) & 0xF];
        last[2] = literals[((size_t)data.len >> 4) & 0xF];
        last[3] = literals[((size_t)data.len >> 0) & 0xF];

        remain += data.len + RG_SIZE(last) + RG_SIZE(tail);

        parts.Append({ last, RG_SIZE(last) });
        parts.Append({ (void *)data.ptr, (size_t)data.len });
        parts.Append({ tail, RG_SIZE(tail) });
    }

    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = parts.data,
        .msg_iovlen = (decltype(msghdr::msg_iovlen))parts.len,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };

    for (;;) {
#if defined(MSG_MORE)
        Size sent = sendmsg(socket->sock, &msg, MSG_MORE | MSG_NOSIGNAL);
#else
        Size sent = sendmsg(socket->sock, &msg, MSG_NOSIGNAL);
#endif

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            request.keepalive = false;
            return false;
        }

        if (sent == remain)
            break;
        remain -= sent;

        for (;;) {
            struct iovec *part = msg.msg_iov;

            if (part->iov_len > (size_t)sent) {
                part->iov_base = (uint8_t *)part->iov_base + sent;
                part->iov_len -= (size_t)sent;

                break;
            }

            msg.msg_iov++;
            msg.msg_iovlen--;
            sent -= (Size)part->iov_len;

            RG_ASSERT(msg.msg_iovlen > 0);
        }
    }

    return true;
}

}

#endif
