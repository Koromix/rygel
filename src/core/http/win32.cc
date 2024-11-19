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

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "server.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>

namespace RG {

static const int BaseAccepts = 512;
static const int MaxAccepts = 16384;

static const Size AcceptAddressLen = 2 * sizeof(SOCKADDR_STORAGE) + 16;

enum class PendingOperation {
    None,
    Accept,
    Disconnect,
    Read,
    Execute,
    Done,

    Manage,
    Exit
};

struct http_Socket {
    int sock = -1;
    bool connected = false;

    PendingOperation op = PendingOperation::None;
    OVERLAPPED overlapped = {};

    uint8_t accept[2 * AcceptAddressLen];
    std::function<void(DWORD)> execute;
    std::function<void()> finalize;

    std::mutex mutex;
    http_IO *client = nullptr;

    ~http_Socket()
    {
        closesocket(sock);

        if (client) {
            delete client;
        }
        if (finalize) {
            finalize();
        }
    };
};

struct IndirectFunctions {
    LPFN_ACCEPTEX AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
    LPFN_DISCONNECTEX DisconnectEx;
};

class http_Dispatcher {
    RG_DELETE_COPY(http_Dispatcher);

    http_Daemon *daemon;
    HANDLE iocp;
    IndirectFunctions fn;

    int listener;

    std::atomic_int pending_accepts { 0 };
    std::atomic_int create_accepts { 0 };
    HeapArray<http_Socket *> sockets;

public:
    http_Dispatcher(http_Daemon *daemon, HANDLE iocp, IndirectFunctions fn, int listener)
        : daemon(daemon), iocp(iocp), fn(fn), listener(listener) {}
    ~http_Dispatcher();

    bool Run();

private:
    bool PostAccept(http_Socket *socket);
    bool PostRead(http_Socket *socket);

    http_Socket *InitSocket();
    void DisconnectSocket(http_Socket *socket);
    void DestroySocket(http_Socket *socket);

    http_IO *InitClient(http_Socket *socket, int64_t start, struct sockaddr *sa);

    friend class http_Daemon;
};

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(!listeners.len);

    if (!InitConfig(config))
        return false;

    int listener = -1;
    RG_DEFER_N(err_guard) { CloseDescriptor(listener); };

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listener = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM | SOCK_OVERLAPPED); } break;
        case SocketType::Unix: { listener = OpenUnixSocket(config.unix_path, SOCK_STREAM | SOCK_OVERLAPPED); } break;
    }
    if (listener < 0)
        return false;

    if (listen(listener, 200) < 0) {
        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    listeners.Append(listener);
    err_guard.Disable();

    if (log_addr) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return true;
}

bool http_Daemon::Start(std::function<void(http_IO *io)> func)
{
    RG_ASSERT(listeners.len == 1);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    int listener = listeners[0];

    RG_DEFER_N(err_guard) {
        if (async) {
            delete async;
            async = nullptr;
        }

        if (iocp) {
            CloseHandle(iocp);
            iocp = nullptr;
        }

        if (dispatcher) {
            delete dispatcher;
            dispatcher = nullptr;
        }
    };

    // Heuristic found on MSDN
    async = new Async(4 * GetCoreCount());

    iocp = CreateIoCompletionPort((HANDLE)(uintptr_t)listener, nullptr, 0, 0);
    if (!iocp) {
        LogError("Failed to create I/O completion port: %1", GetWin32ErrorString());
        return false;
    }

    IndirectFunctions fn;
    {
        static const GUID AcceptExGuid = WSAID_ACCEPTEX;
        static const GUID GetAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;
        static const GUID DisconnectExGuid = WSAID_DISCONNECTEX;

        DWORD dummy;

        if (WSAIoctl((SOCKET)listener, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&AcceptExGuid, RG_SIZE(AcceptExGuid), 
                     &fn.AcceptEx, RG_SIZE(fn.AcceptEx),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();
            LogError("Failed to load AcceptEx() function: %1", strerror(errno));
            return false;
        }

        if (WSAIoctl((SOCKET)listener, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&GetAcceptExSockaddrsGuid, RG_SIZE(GetAcceptExSockaddrsGuid), 
                     &fn.GetAcceptExSockaddrs, RG_SIZE(fn.GetAcceptExSockaddrs),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();
            LogError("Failed to load GetAcceptExSockaddrs() function: %1", strerror(errno));
            return false;
        }

        if (WSAIoctl((SOCKET)listener, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&DisconnectExGuid, RG_SIZE(DisconnectExGuid), 
                     &fn.DisconnectEx, RG_SIZE(fn.DisconnectEx),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();
            LogError("Failed to load DisconnectEx() function: %1", strerror(errno));
            return false;
        }
    }

    dispatcher = new http_Dispatcher(this, iocp, fn, listener);

    // Prepare sockets
    for (Size i = 0; i < BaseAccepts; i++) {
        http_Socket *socket = dispatcher->InitSocket();

        if (!dispatcher->PostAccept(socket))
            return false;
    }

    // Cannot fail anymore
    err_guard.Disable();

    handle_func = func;

    for (Size i = 0; i < async->GetWorkerCount(); i++) {
        async->Run([this] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    if (async) {
        for (Size i = 0; i < async->GetWorkerCount(); i++) {
            PostQueuedCompletionStatus(iocp, 0, (int)PendingOperation::Exit, nullptr);
        }

        async->Sync();

        delete async;
        async = nullptr;
    }

    if (dispatcher) {
        delete dispatcher;
        dispatcher = nullptr;
    }

    for (int listener: listeners) {
        CloseSocket(listener);
    }
    listeners.Clear();

    if (iocp) {
        CloseHandle(iocp);
        iocp = nullptr;
    }

    handle_func = {};
}

void http_Daemon::StartRead(http_Socket *)
{
    // Nothing to do
}

void http_Daemon::StartWrite(http_Socket *)
{
    // Nothing to do
}

void http_Daemon::EndWrite(http_Socket *socket)
{
    socket->op = PendingOperation::Done;
    PostQueuedCompletionStatus(iocp, 0, 0, &socket->overlapped);
}

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
    int len = (int)std::min(buf.len, (Size)INT_MAX);
    Size bytes = recv((SOCKET)socket->sock, (char *)buf.ptr, len, 0);

    if (bytes < 0) {
        errno = TranslateWinSockError();
        if (errno != ENOTCONN && errno != ECONNRESET) {
            LogError("Failed to read from client: %1", strerror(errno));
        }
        return -1;
    }

    return bytes;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf)
{
    while (buf.len) {
        int len = (int)std::min(buf.len, (Size)INT_MAX);
        int bytes = send(socket->sock, (char *)buf.ptr, len, 0);

        if (bytes < 0) {
            errno = TranslateWinSockError();
            if (errno != ENOTCONN && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        buf.ptr += bytes;
        buf.len -= bytes;
    }

    return true;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> parts)
{
    while (parts.len) {
        LocalArray<WSABUF, 64> bufs;
        bufs.len = std::min(parts.len, bufs.Available());

        for (Size i = 0; i < bufs.len; i++) {
            const Span<const uint8_t> &part = parts[i];

            if (part.len > (Size)INT_MAX) [[unlikely]] {
                LogError("Cannot proceed with excessive scattered chunk size");
                return false;
            }

            bufs[i].buf = (char *)part.ptr;
            bufs[i].len = (unsigned long)part.len;
        }

        DWORD sent = 0;
        int ret = WSASend((SOCKET)socket->sock, bufs.data, (DWORD)bufs.len, &sent, 0, nullptr, nullptr);

        if (ret) {
            errno = TranslateWinSockError();
            if (errno != ENOTCONN && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        // Windows does not apparently do partial writes, so don't bother dealing with that.
        // Go on!

        parts.ptr += bufs.len;
        parts.len -= bufs.len;
    }

    return true;
}

static http_Socket *SocketFromOverlapped(void *ptr)
{
    uint8_t *data = (uint8_t *)ptr;
    return (http_Socket *)(data - offsetof(http_Socket, overlapped));
}

http_Dispatcher::~http_Dispatcher()
{
    for (http_Socket *socket: sockets) {
        delete socket;
    }

    sockets.Clear();
}

bool http_Dispatcher::Run()
{
    for (;;) {
        DWORD transferred;
        uintptr_t key;
        OVERLAPPED *overlapped;

        bool success = GetQueuedCompletionStatus(iocp, &transferred, &key, &overlapped, INFINITE);

        if (!success && !overlapped) {
            LogError("GetQueuedCompletionStatus() failed: %1", GetWin32ErrorString());
            return false;
        }

        int64_t now = GetMonotonicTime();
        http_Socket *socket = overlapped ? SocketFromOverlapped(overlapped) : nullptr;
        PendingOperation op = socket ? socket->op : (PendingOperation)key;

        switch (op) {
            case PendingOperation::None: { RG_UNREACHABLE(); } break;

            case PendingOperation::Accept: {
                RG_ASSERT(socket);

                std::lock_guard<std::mutex> lock(socket->mutex);
                socket->op = PendingOperation::None;

                if (--pending_accepts < BaseAccepts) {
                    bool post = !create_accepts.fetch_add(1);

                    if (post) {
                        PostQueuedCompletionStatus(iocp, 0, (int)PendingOperation::Manage, nullptr);
                    }
                }

                if (!success) {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = true;
                setsockopt(socket->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&listener, RG_SIZE(listener));

                sockaddr *local_addr = nullptr;
                sockaddr *remote_addr = nullptr;
                int local_len;
                int remote_len;

                fn.GetAcceptExSockaddrs(socket->accept, 0, AcceptAddressLen, AcceptAddressLen,
                                        &local_addr, &local_len, &remote_addr, &remote_len);

                http_IO *client = InitClient(socket, now, remote_addr);
                if (!client) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                RG_ASSERT(!socket->client);
                socket->client = client;

                if (!PostRead(socket)) {
                    DisconnectSocket(socket);
                }
            } break;

            case PendingOperation::Disconnect: {
                RG_ASSERT(socket);

                std::lock_guard<std::mutex> lock(socket->mutex);
                socket->op = PendingOperation::None;

                if (!success) [[unlikely]] {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = false;

                bool reuse = create_accepts.load(std::memory_order_relaxed) > 4 ||
                             pending_accepts.load(std::memory_order_relaxed) < BaseAccepts * 2;

                if (!reuse || !PostAccept(socket)) {
                    DestroySocket(socket);
                }
            } break;

            case PendingOperation::Read: {
                RG_ASSERT(socket);

                std::lock_guard<std::mutex> lock(socket->mutex);
                socket->op = PendingOperation::None;

                if (!success) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                http_IO *client = socket->client;

                client->incoming.buf.len += (Size)transferred;
                client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                http_RequestStatus status = client->ParseRequest();

                switch (status) {
                    case http_RequestStatus::Busy: {
                        if (!PostRead(socket)) {
                            DisconnectSocket(socket);
                        }
                    } break;

                    case http_RequestStatus::Ready: { daemon->RunHandler(client, now); } break;

                    case http_RequestStatus::Close: { DisconnectSocket(socket); } break;
                }
            } break;

            case PendingOperation::Execute: {
                RG_ASSERT(socket);

                std::lock_guard<std::mutex> lock(socket->mutex);
                socket->op = PendingOperation::None;

                if (!success) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                socket->execute(transferred);
            } break;

            case PendingOperation::Done: {
                RG_ASSERT(socket);

                std::lock_guard<std::mutex> lock(socket->mutex);
                socket->op = PendingOperation::None;

                if (!success) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                http_IO *client = socket->client;

                if (!client->Rearm(now) || !PostRead(socket)) {
                    DisconnectSocket(socket);
                }
            } break;

            case PendingOperation::Manage: {
                Size start_len = sockets.len;
                int prev_create = 0;

                do {
                    int create = create_accepts;
                    int pending = pending_accepts;

                    int target = std::clamp(pending + 4 * (create - prev_create), BaseAccepts + 64, MaxAccepts - pending);
                    int failures = 0;

                    for (int i = pending; i < target; i++) {
                        http_Socket *socket = InitSocket();

                        if (!PostAccept(socket)) [[unlikely]] {
                            if (++failures >= 8) {
                                LogError("System starvation, giving up");
                                return false;
                            }

                            WaitDelay(20);
                        }
                    }

                    prev_create = create;
                } while (pending_accepts < BaseAccepts);

                Size j = 0;
                for (Size i = 0; i < start_len; i++) {
                    sockets[j] = sockets[i];
                    j += (sockets[i]->sock >= 0);
                }
                for (Size i = start_len; i < sockets.len; i++, j++) {
                    sockets[j] = sockets[i];
                }
                sockets.len = j;

                // Reset last to make sure only one Manage operations runs at the same time
                create_accepts = 0;
            } break;

            case PendingOperation::Exit: {
                RG_ASSERT(success);
                return true;
            } break;

        }
    }

    RG_UNREACHABLE();
}

bool http_Dispatcher::PostAccept(http_Socket *socket)
{
    DWORD dummy = 0;

    socket->op = PendingOperation::Accept;

retry:
    if (!fn.AcceptEx((SOCKET)listener, (SOCKET)socket->sock, socket->accept, 0,
                     AcceptAddressLen, AcceptAddressLen, &dummy, &socket->overlapped) &&
            WSAGetLastError() != WSA_IO_PENDING) {
        errno = TranslateWinSockError();

        if (errno == ECONNRESET)
            goto retry;

        LogError("Failed to issue socket accept operation: %1", strerror(errno));
        return false;
    }

    pending_accepts++;

    return true;
}

bool http_Dispatcher::PostRead(http_Socket *socket)
{
    if (socket->op == PendingOperation::Read)
        return true;

    RG_ASSERT(socket->op == PendingOperation::None);
    RG_ASSERT(socket->client);

    http_IO *client = socket->client;

    client->incoming.buf.Grow(Kibibytes(8));

    WSABUF buf = { (unsigned long)client->incoming.buf.Available() - 1, (char *)client->incoming.buf.end() };
    DWORD received = 0;
    DWORD flags = 0;

    socket->op = PendingOperation::Read;

    if (WSARecv((SOCKET)socket->sock, &buf, 1, &received, &flags, &socket->overlapped, nullptr) &&
            WSAGetLastError() != WSA_IO_PENDING) {
        errno = TranslateWinSockError();
        if (errno != ENOTCONN && errno != ECONNRESET) {
            LogError("Failed to read from socket: %1", strerror(errno));
        }
        return false;
    }

    return true;
}

// Only call from one thread at a time
http_Socket *http_Dispatcher::InitSocket()
{
    http_Socket *socket = new http_Socket();
    RG_DEFER_N(err_guard) { delete socket; };

    socket->sock = CreateSocket(daemon->sock_type, SOCK_STREAM | SOCK_OVERLAPPED);
    if (socket->sock < 0)
        return nullptr;

    if (!CreateIoCompletionPort((HANDLE)(uintptr_t)socket->sock, iocp, 0, 0)) {
        LogError("Failed to associate socket with IOCP: %1", GetWin32ErrorString());
        return nullptr;
    }

    err_guard.Disable();
    sockets.Append(socket);

    return socket;
}

void http_Dispatcher::DisconnectSocket(http_Socket *socket)
{
    if (!socket)
        return;

    RG_ASSERT(socket->connected);

    if (socket->client) {
        delete socket->client;
        socket->client = nullptr;
    }
    if (socket->finalize) {
        socket->finalize();
        socket->finalize = {};
    }

    socket->op = PendingOperation::Disconnect;

    if (!fn.DisconnectEx((SOCKET)socket->sock, &socket->overlapped, TF_REUSE_SOCKET, 0) &&
            WSAGetLastError() != WSA_IO_PENDING) {
        errno = TranslateWinSockError();
        if (errno != ENOTCONN) {
            LogError("Failed to reuse socket: %1", strerror(errno));
        }
        DestroySocket(socket);
    }
}

void http_Dispatcher::DestroySocket(http_Socket *socket)
{
    if (!socket)
        return;

    socket->~http_Socket();
    socket->sock = -1;
    socket->connected = false;
    socket->op = PendingOperation::None;

    // If anything fails (should be very rare), we're temporarily leaking the struct until
    // a cleanup happens when more sockets are created (see PendingOperation::Manage).

    socket->sock = CreateSocket(daemon->sock_type, SOCK_STREAM | SOCK_OVERLAPPED);
    if (socket->sock < 0)
        return;

    if (!CreateIoCompletionPort((HANDLE)(uintptr_t)socket->sock, iocp, 0, 0)) {
        LogError("Failed to associate socket with IOCP: %1", GetWin32ErrorString());
        return;
    }
}

http_IO *http_Dispatcher::InitClient(http_Socket *socket, int64_t start, struct sockaddr *sa)
{
    http_IO *client = new http_IO(daemon);

    if (!client->Init(socket, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    static const Size MaxSend = Mebibytes(2);

    RG_ASSERT(socket);
    RG_ASSERT(!response.started);
    RG_ASSERT(len >= 0);

    response.started = true;

    RG_ASSERT(!socket->finalize);
    socket->finalize = [=]() { _close(fd); };

    // Send intro and file in one go
    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);
    TRANSMIT_FILE_BUFFERS tbuf = { (void *)intro.ptr, (DWORD)intro.len, nullptr, 0 };

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    int64_t offset = -intro.len;
    int64_t remain = intro.len + len;

    if (remain > MaxSend) {
        socket->execute = [=, this](DWORD transferred) mutable {
            offset += (Size)transferred;
            remain -= (Size)transferred;

            if (offset < 0) [[unlikely]] {
                LogError("TransmitFile() stopped too early");

                request.keepalive = false;

                socket->op = PendingOperation::Done;
                PostQueuedCompletionStatus(daemon->iocp, 0, 0, &socket->overlapped);

                return;
            }

            socket->op = remain ? PendingOperation::Execute : PendingOperation::Done;
            socket->overlapped.OffsetHigh = (DWORD)(offset >> 32);
            socket->overlapped.Offset = (DWORD)(offset & 0xFFFFFFFFu);

            DWORD send = (DWORD)std::min(remain, MaxSend);

            if (!TransmitFile((SOCKET)socket->sock, h, send, 0, &socket->overlapped, nullptr, 0) &&
                    WSAGetLastError() != WSA_IO_PENDING) [[unlikely]] {
                errno = TranslateWinSockError();
                LogError("Failed to send file: %1", strerror(errno));

                request.keepalive = false;

                socket->op = PendingOperation::Done;
                PostQueuedCompletionStatus(daemon->iocp, 0, 0, &socket->overlapped);

                return;
            }
        };
        socket->op = PendingOperation::Execute;
    } else {
        socket->op = PendingOperation::Done;
    }

    socket->overlapped.OffsetHigh = 0;
    socket->overlapped.Offset = 0;

    DWORD send = (DWORD)(std::min(remain, MaxSend) - intro.len);

    if (!TransmitFile((SOCKET)socket->sock, h, send, 0, &socket->overlapped, &tbuf, 0) &&
            WSAGetLastError() != WSA_IO_PENDING) [[unlikely]] {
        errno = TranslateWinSockError();
        LogError("Failed to send file: %1", strerror(errno));

        request.keepalive = false;

        socket->op = PendingOperation::Done;
        PostQueuedCompletionStatus(daemon->iocp, 0, 0, &socket->overlapped);
    }
}

}

#endif
