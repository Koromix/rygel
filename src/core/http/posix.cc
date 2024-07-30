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
#include "posix.hh"

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
#if defined(__linux__)
    #include <sys/sendfile.h>
#else
    #include <sys/uio.h>
#endif

namespace RG {

// Sane platform
static_assert(EAGAIN == EWOULDBLOCK);

Size http_IO::ReadDirect(Span<uint8_t> data)
{
    Size total = 0;

    if (incoming.extra.len) {
        int64_t remaining = request.body_len - incoming.read;
        Size copy_len = (Size)std::min((int64_t)std::min(incoming.extra.len, data.len), remaining);

        MemCpy(data.ptr, incoming.extra.ptr, copy_len);
        incoming.extra.ptr += copy_len;
        incoming.extra.len -= copy_len;

        total += copy_len;
        incoming.read += copy_len;

        if (copy_len == data.len)
            return total;

        data.ptr += copy_len;
        data.len -= copy_len;
    }

    if (incoming.read == request.body_len)
        return total;
    RG_ASSERT(incoming.read < request.body_len);

    int64_t remaining = request.body_len - incoming.read;
    Size limit = (Size)std::min((int64_t)data.len, remaining);

restart:
    Size read = recv(socket->sock, data.ptr, limit, 0);

    if (read < 0) {
        if (errno == EINTR)
            goto restart;

        if (errno != EPIPE && errno != ECONNRESET) {
            LogError("Failed to send to client: %1", strerror(errno));
        }

        request.keepalive = false;
        return -1;
    }

    total += read;
    incoming.read += read;

    return total;
}

static bool SendFull(int sock, Span<const uint8_t> buf)
{
    while (buf.len) {
#if defined(MSG_MORE)
        Size sent = send(sock, buf.ptr, (size_t)buf.len, MSG_MORE | MSG_NOSIGNAL);
#else
        Size sent = send(sock, buf.ptr, (size_t)buf.len, MSG_NOSIGNAL);
#endif

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        buf.ptr += sent;
        buf.len -= sent;
    }

    return true;
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    RG_ASSERT(socket);
    RG_ASSERT(!response.started);
    RG_ASSERT(len >= 0);

    RG_DEFER { close(fd); };

    response.started = true;
    response.expected = len;

#if defined(__linux__)
    RG_DEFER { SetSocketRetain(socket->sock, false); };

    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    if (!SendFull(socket->sock, intro.As<uint8_t>())) {
        request.keepalive = false;
        return;
    }

    off_t offset = 0;
    int64_t remain = len;

    while (remain) {
        Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);
        Size sent = sendfile(socket->sock, fd, &offset, (size_t)send);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            if (errno != EPIPE) {
                LogError("Failed to send file: %1", strerror(errno));
            }

            request.keepalive = false;
            return;
        }

        if (!sent) [[unlikely]] {
            LogError("Truncated file sent");

            request.keepalive = false;
            return;
        }

        remain -= sent;
    }
#elif defined(__FreeBSD__) || defined(__APPLE__)
#if !defined(MSG_DONTWAIT)
    SetSocketNonBlock(socket->sock, false);
    RG_DEFER { SetSocketNonBlock(socket->sock, true); };
#endif

    SetSocketRetain(socket->sock, true);
    RG_DEFER { SetSocketRetain(socket->sock, false); };

    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    struct iovec header = {};
    struct sf_hdtr hdtr = { &header, 1, nullptr, 0 };

    off_t offset = 0;
    int64_t remain = intro.len + len;

    // Send intro and file in one go
    while (remain) {
        if (offset < intro.len) {
            header.iov_base = (void *)(intro.ptr + offset);
            header.iov_len = (size_t)(intro.len - offset);
        } else {
            hdtr.headers = nullptr;
            hdtr.hdr_cnt = 0;
        }

        Size send = (Size)std::min(remain, (int64_t)RG_SIZE_MAX);

#if defined(__FreeBSD__)
        off_t sent = 0;
        int ret = sendfile(fd, socket->sock, offset, (size_t)send, &hdtr, &sent, 0);
#else
        off_t sent = (off_t)send;
        int ret = sendfile(fd, socket->sock, offset, &sent, &hdtr, 0);
#endif

        if (ret < 0 && errno != EINTR) {
            if (errno != EPIPE) {
                LogError("Failed to send file: %1", strerror(errno));
            }

            request.keepalive = false;
            return;
        }

        if (!ret && !sent) [[unlikely]] {
            LogError("Truncated file sent");

            request.keepalive = false;
            return;
        }

        offset += sent;
        remain -= (int64_t)sent;
    }
#else
    Send(status, len, [&](StreamWriter *writer) {
        StreamReader reader(fd, "<file>");

        if (!SpliceStream(&reader, len, writer)) {
            request.keepalive = false;
            return false;
        }
        if (writer->IsValid() && writer->GetRawWritten() < len) {
            LogError("File was truncated while sending");

            request.keepalive = false;
            return false;
        }

        return true;
    });
#endif
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    if (!data.len) {
        bool success = (response.expected < 0 || response.sent == response.expected);

        SetSocketRetain(socket->sock, false);
#if !defined(MSG_DONTWAIT)
        SetSocketNonBlock(socket->sock, false);
#endif

        request.keepalive &= success;
        return success;
    }

    if (!SendFull(socket->sock, data)) {
        request.keepalive = false;
        return false;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    if (!data.len) {
        bool success = (response.expected < 0 || response.sent == response.expected);

        uint8_t end[5] = { '0', '\r', '\n', '\r', '\n' };
        success &= SendFull(socket->sock, end);

        SetSocketRetain(socket->sock, false);
#if !defined(MSG_DONTWAIT)
        SetSocketNonBlock(socket->sock, false);
#endif

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
