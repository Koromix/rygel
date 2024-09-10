// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

void http_Daemon::StartRead(http_Socket *socket)
{
#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(socket->sock, false);
#else
    (void)socket;
#endif
}

void http_Daemon::StartWrite(http_Socket *socket)
{
#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(socket->sock, false);
#endif
    SetDescriptorRetain(socket->sock, true);
}

void http_Daemon::EndWrite(http_Socket *socket)
{
    SetDescriptorRetain(socket->sock, false);
}

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
restart:
    Size bytes = recv(socket->sock, buf.ptr, (size_t)buf.len, 0);

    if (bytes < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to read from client: %1", strerror(errno));
        }

        socket->client.request.keepalive = false;
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

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            socket->client.request.keepalive = false;
            return false;
        }

        socket->client.timeout_at = GetMonotonicTime() + send_timeout;

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    return true;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> parts)
{
    static_assert(RG_SIZE(Span<const uint8_t>) == RG_SIZE(struct iovec));
    static_assert(alignof(Span<const uint8_t>) == alignof(struct iovec));
    static_assert(offsetof(Span<const uint8_t>, ptr) == offsetof(struct iovec, iov_base));
    static_assert(offsetof(Span<const uint8_t>, len) == offsetof(struct iovec, iov_len));

    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = (struct iovec *)parts.ptr,
        .msg_iovlen = (decltype(msghdr::msg_iovlen))parts.len,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };
    int flags = MSG_NOSIGNAL;

#if defined(MSG_MORE)
    flags |= MSG_MORE;
#endif

    while (msg.msg_iovlen) {
        Size sent = sendmsg(socket->sock, &msg, flags);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EINVAL && errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }

            socket->client.request.keepalive = false;
            return false;
        }

        socket->client.timeout_at = GetMonotonicTime() + send_timeout;

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

}

#endif
