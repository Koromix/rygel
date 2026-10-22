// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(_WIN32)

#include "lib/native/base/base.hh"
#include "server.hh"
#include "ws.hh"

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

#define BEGIN_IGNORE_MISSING_FIELDS \
   _Pragma("GCC diagnostic push") \
   _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"")
#define END_IGNORE_MISSING_FIELDS \
   _Pragma("GCC diagnostic pop")

namespace K {

struct http_Socket {
    int dispatcher;

    int sock = -1;
    bool process = false;
    bool poll = false;
    bool handled = true;
    bool nodelay = false;
    int lingered = 0;

    HANDLE read_ev = nullptr;
    HANDLE write_ev = nullptr;

    http_IO client;

    http_Socket(http_Daemon *daemon, int dispatcher)
        : dispatcher(dispatcher), client(daemon) {}
    ~http_Socket();
};

static const int MaxDispatcherClients = 128;
static const int MaxDispatcherWebSockets = 1024;
static const Size MaxSend = Mebibytes(1);

class http_Dispatcher {
    http_Daemon *daemon;
    int dispatcher;
    int listener;

    int pair_fd[2] = { -1, -1 };

    HeapArray<http_Socket *> sockets;
    LocalArray<http_Socket *, 64> free_sockets;
    RetainPtr<ws_Registry> websockets;

public:
    http_Dispatcher(http_Daemon *daemon, int dispatcher, int listener)
        : daemon(daemon), dispatcher(dispatcher), listener(listener) {}
    ~http_Dispatcher();

    bool Init();
    void Run();

    void Wake(http_Socket *socket, bool done);

private:
    http_Socket *InitSocket(SOCKET sock, int64_t start, struct sockaddr *sa);
    void ParkSocket(http_Socket *socket, bool hard);

    friend class http_Daemon;
};

http_Dispatcher *http_Daemon::CreateDispatcher(int idx, int listener)
{
    http_Dispatcher *dispatcher = new http_Dispatcher(this, idx, listener);

    if (!dispatcher->Init()) {
        delete dispatcher;
        return nullptr;
    }

    return dispatcher;
}

void http_Daemon::RunDispatcher(http_Dispatcher *dispatcher)
{
    dispatcher->Run();
}

void http_Daemon::StopDispatcher(http_Dispatcher *dispatcher)
{
    dispatcher->Wake(nullptr, false);
}

void http_Daemon::DestroyDispatcher(http_Dispatcher *dispatcher)
{
    delete dispatcher;
}

void http_Daemon::StartRead(http_Socket *)
{
    // Nothing to do
}

void http_Daemon::StartWrite(http_Socket *socket, bool)
{
    // Nothing to do

    if (!socket->nodelay) {
        // Setting TCP_NODELAY just after accept() can fail, when the socket is still in CONNECTING state.
        // Now it is safe!
        int one = 1;
        setsockopt(socket->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));

        socket->nodelay = true;
    }
}

void http_Daemon::EndWrite(http_Socket *)
{
    // Nothing to do
}

Size http_Daemon::ReadSocket(http_Socket *socket, Span<uint8_t> buf)
{
    int error = 0;

BEGIN_IGNORE_MISSING_FIELDS
    WSAOVERLAPPED ov = { .hEvent = socket->read_ev };
    WSABUF wbuf = { (unsigned long)std::min(buf.len, (Size)ULONG_MAX), (char *)buf.ptr };
    DWORD flags = 0;
    DWORD bytes = 0;
END_IGNORE_MISSING_FIELDS

    if (WSARecv((SOCKET)socket->sock, &wbuf, 1, &bytes, &flags, &ov, nullptr) == SOCKET_ERROR) {
        error = GetLastError();

        if (error != WSA_IO_PENDING)
            goto error;

        if (!WSAGetOverlappedResult((SOCKET)socket->sock, &ov, &bytes, true, &flags)) {
            error = GetLastError();
            goto error;
        }
    }

    socket->client.ExtendTimeout(bytes / 4, max_timeout);

    return (Size)bytes;

error:
    if (error != WSAENOTCONN && error != WSAECONNRESET && error != WSAESHUTDOWN && error != WSA_OPERATION_ABORTED) {
        LogError("Failed to read from client: %1", GetWin32ErrorString(error));
    }

    socket->client.keepalive = false;
    return -1;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<const uint8_t> buf, bool nowait)
{
    int error = 0;

    while (buf.len) {
BEGIN_IGNORE_MISSING_FIELDS
        WSAOVERLAPPED ov = { .hEvent = socket->write_ev };
        WSABUF wbuf = { (unsigned long)std::min(buf.len, MaxSend), (char *)buf.ptr };
        DWORD flags = 0;
        DWORD bytes = 0;
END_IGNORE_MISSING_FIELDS

        // Extend beforehand to prevent premature timeout
        socket->client.ExtendTimeout(wbuf.len / 4, max_timeout);

        if (WSASend((SOCKET)socket->sock, &wbuf, 1, &bytes, flags, nowait ? nullptr : &ov, nullptr) == SOCKET_ERROR) {
            error = GetLastError();

            if (error != WSA_IO_PENDING)
                goto error;

            if (!WSAGetOverlappedResult((SOCKET)socket->sock, &ov, &bytes, true, &flags)) {
                error = GetLastError();
                goto error;
            }
        }

        buf.ptr += (Size)bytes;
        buf.len -= (Size)bytes;
    }

    return true;

error:
    if (error != WSAENOTCONN && error != WSAECONNRESET && error != WSAESHUTDOWN && error != WSA_OPERATION_ABORTED) {
        LogError("Failed to send to client: %1", GetWin32ErrorString(error));
    }

    socket->client.keepalive = false;
    return false;
}

bool http_Daemon::WriteSocket(http_Socket *socket, Span<Span<const uint8_t>> parts)
{
    int error = 0;

    while (parts.len) {
        LocalArray<WSABUF, 64> buffers;
        Size send = 0;

        buffers.len = std::min(parts.len, K_LEN(buffers.data));

        for (Size i = 0; i < buffers.len; i++) {
            const Span<const uint8_t> &part = parts[i];

            if (part.len > (Size)INT_MAX) [[unlikely]] {
                LogError("Cannot proceed with excessive scattered chunk size");

                socket->client.keepalive = false;
                return false;
            }

            buffers[i].buf = (char *)part.ptr;
            buffers[i].len = (unsigned long)part.len;

            send += part.len;
        }

        // Extend beforehand to prevent premature timeout
        socket->client.ExtendTimeout(send / 4, max_timeout);

BEGIN_IGNORE_MISSING_FIELDS
        WSAOVERLAPPED ov = { .hEvent = socket->write_ev };
        DWORD flags = 0;
        DWORD sent = 0;
END_IGNORE_MISSING_FIELDS

        if (WSASend((SOCKET)socket->sock, buffers.data, (DWORD)buffers.len, &sent, flags, &ov, nullptr) == SOCKET_ERROR) {
            error = GetLastError();

            if (error != WSA_IO_PENDING)
                goto error;

            if (!WSAGetOverlappedResult((SOCKET)socket->sock, &ov, &sent, true, &flags)) {
                error = GetLastError();
                goto error;
            }
        }

        do {
            Span<const uint8_t> *part = parts.ptr;

            if (part->len > (Size)sent) {
                part->ptr += sent;
                part->len -= (Size)sent;

                break;
            }

            parts.ptr++;
            parts.len--;
            sent -= (DWORD)part->len;
        } while (parts.len);
    }

    return true;

error:
    if (error != WSAENOTCONN && error != WSAECONNRESET && error != WSAESHUTDOWN && error != WSA_OPERATION_ABORTED) {
        LogError("Failed to send to client: %1", GetWin32ErrorString(error));
    }

    socket->client.keepalive = false;
    return false;
}

bool http_Daemon::GiveSocket(http_Socket *socket, ws_Socket *ws)
{
    http_Dispatcher *dispatcher = dispatchers[socket->dispatcher].dispatcher;
    return dispatcher->websockets->Give(socket->sock, ws);
}

static bool CreateSocketPair(int out_pair[2])
{
    for (int i = 0; i < 8; i++) {
        SOCKET listener = INVALID_SOCKET;
        SOCKET client = INVALID_SOCKET;
        SOCKET peer = INVALID_SOCKET;

        K_DEFER {
            closesocket(listener);
            closesocket(client);
            closesocket(peer);
        };

        sockaddr_in addr = {};
        socklen_t addr_len = K_SIZE(addr);

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
            break;
        client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client == INVALID_SOCKET)
            break;

        // Set reuse flag
        {
            int reuse = 1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, K_SIZE(reuse));
        }

        if (bind(listener, (struct sockaddr *)&addr, K_SIZE(addr)) < 0)
            break;
        if (getsockname(listener, (struct sockaddr *)&addr, &addr_len) < 0)
            break;
        if (listen(listener, 1) < 0)
            break;
        if (connect(client, (struct sockaddr *)&addr, K_SIZE(addr)) < 0)
            break;

        peer = accept(listener, nullptr, nullptr);
        if (peer == INVALID_SOCKET)
            break;

        // Make sure no hijacking occured
        {
            sockaddr_in addr1 = {};
            sockaddr_in addr2 = {};
            socklen_t addr1_len = K_SIZE(addr1);
            socklen_t addr2_len = K_SIZE(addr2);

            if (getsockname(client, (struct sockaddr *)&addr1, &addr1_len) < 0)
                break;
            if (getpeername(peer, (struct sockaddr *)&addr2, &addr2_len) < 0)
                break;

            if (addr1_len != addr2_len || memcmp(&addr1, &addr2, addr1_len)) {
                SetLastError(ERROR_NETWORK_BUSY); // Whatever
                continue;
            }
        }

        // Success!
        out_pair[0] = (int)client;
        out_pair[1] = (int)peer;
        client = INVALID_SOCKET;
        peer = INVALID_SOCKET;

        return true;
    }

    LogError("Failed to create socket pair: %1", GetWin32ErrorString());
    return false;
}

http_Socket::~http_Socket()
{
    CloseSocket(client.HasLeaked() ? -1 : sock);

    if (read_ev) {
        CloseHandle(read_ev);
    }
    if (write_ev) {
        CloseHandle(write_ev);
    }
}

http_Dispatcher::~http_Dispatcher()
{
    CloseSocket(pair_fd[0]);
    CloseSocket(pair_fd[1]);
}

bool http_Dispatcher::Init()
{
    K_ASSERT(pair_fd[0] < 0);

    if (!CreateSocketPair(pair_fd))
        return false;
    SetSocketNonBlock(pair_fd[0], true);

    websockets = RetainPtr<ws_Registry>::New(MaxDispatcherWebSockets);

    return true;
}

void http_Dispatcher::Run()
{
    K_ASSERT(pair_fd[0] >= 0);

    SetThreadName("HTTP dispatcher");

    Async async(daemon->async);

    // Delete remaining clients when function exits
    K_DEFER {
        bool finished = async.Wait(100);

        if (!finished) {
            LogInfo("Waiting up to %1 sec before shutting down clients...", (double)daemon->stop_timeout / 1000);
            finished = async.Wait(daemon->stop_timeout);
        }

        websockets->Shutdown();

        if (!finished) {
            for (http_Socket *socket: sockets) {
                if (socket->client.StartLinger() < 0)
                    continue;

                shutdown((SOCKET)socket->sock, SD_BOTH);
                CancelIoEx((HANDLE)(SOCKET)socket->sock, nullptr);
            }

            // I/O submission runs in worker threads, and can race with shutdown/cancellation,
            // so keep canceling overlapped I/O operations until all handlers have exited.
            while (!async.Wait(100)) {
                for (http_Socket *socket: sockets) {
                    if (socket->client.StartLinger() < 0)
                        continue;

                   CancelIoEx((HANDLE)(SOCKET)socket->sock, nullptr);
                }
            }
        }

        for (http_Socket *socket: sockets) {
            delete socket;
        }
        for (http_Socket *socket: free_sockets) {
            delete socket;
        }

        sockets.Clear();
        free_sockets.Clear();
    };

    HeapArray<struct pollfd> pfd_objs;
    HeapArray<http_Socket *> pfd_sockets;
    bool shedding = false;

    // React to connections
    pfd_objs.Append({ (SOCKET)listener, POLLIN, 0 });
    pfd_objs.Append({ (SOCKET)pair_fd[0], POLLIN, 0 });
    pfd_sockets.AppendDefault(2);

    for (;;) {
        int64_t clock = GetMonotonicClock();
        bool accepts = pfd_objs[0].revents;

        if (pfd_objs[1].revents) {
            bool exit = false;

            for (;;) {
                uintptr_t addr = 0;
                Size ret = recv(pair_fd[0], (char *)&addr, K_SIZE(addr), 0);

                if (ret < 0) [[unlikely]] {
                    if (GetLastError() == WSAEWOULDBLOCK)
                        break;

                    LogError("Unexpected error during pipe read: %1", GetWin32ErrorString());
                    return;
                } else if (ret != K_SIZE(addr)) [[unlikely]] {
                    LogError("Unexpected empty or partial read during pipe read");
                    return;
                }

                http_Socket *socket = (http_Socket *)(addr & ~1ull);
                bool done = addr & 1;

                if (!socket) [[unlikely]] {
                    exit = true;
                    continue;
                }

                socket->handled = true;

                http_IO *client = &socket->client;

                if (done) {
                    if (client->StartLinger() < 0) {
                        client->SetTimeout(clock);
                        continue;
                    }

                    shutdown((SOCKET)socket->sock, SD_SEND);
                }

                socket->poll = true;
            }

            if (exit)
                return;
        }
        for (Size i = 2; i < pfd_objs.len; i++) {
            struct pollfd &pfd = pfd_objs[i];
            http_Socket *socket = pfd_sockets[i];

            socket->process |= !!pfd.revents;
        }

        // Process new connections
        if (accepts) {
            for (int i = 0; i < 32; i++) {
                sockaddr_storage ss;
                socklen_t ss_len = K_SIZE(ss);

                SOCKET sock = accept(listener, (sockaddr *)&ss, &ss_len);

                if (sock == INVALID_SOCKET) {
                    int error = GetLastError();

                    if (error == WSAEWOULDBLOCK)
                        break;
                    if (error == WSAEINVAL)
                        return;

                    // Assume transient error (such as too many open files), slow down
                    LogError("Failed to accept client: %1", GetWin32ErrorString());
                    WaitDelay(20);

                    break;
                }

                if (sockets.len >= MaxDispatcherClients) [[unlikely]] {
                    if (!shedding) {
                        LogWarning("Shedding HTTP connections (capacity overload)");
                        shedding = true;
                    }

                    struct linger sl = { 1, 0 };
                    setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
                    CloseSocket((int)sock);

                    continue;
                } else {
                    shedding = false;
                }

                SetSocketNonBlock((int)sock, true);

                http_Socket *socket = InitSocket(sock, clock, (sockaddr *)&ss);

                if (!socket) [[unlikely]]
                    continue;

                sockets.Append(socket);
            }
        }

        Size keep = 0;
        unsigned int timeout = UINT_MAX;

        // Process clients
        for (Size i = 0; i < sockets.len; i++, keep++) {
            sockets[keep] = sockets[i];

            http_Socket *socket = sockets[i];
            http_IO *client = &socket->client;
            http_RequestStatus status = http_RequestStatus::Busy;

            if (socket->process) {
                socket->process = false;

                client->incoming.buf.Grow(Kibibytes(8));

                Size available = client->incoming.buf.Available() - 1;
                Size bytes = recv(socket->sock, (char *)client->incoming.buf.end(), (int)available, 0);

                if (bytes > 0) {
                    int linger = client->linger.load(std::memory_order_relaxed);

                    if (linger) {
                        socket->lingered += (int)bytes;

                        if (socket->lingered > linger) {
                            ParkSocket(socket, true);
                            keep--;

                            continue;
                        }
                    } else {
                        client->incoming.buf.len += bytes;
                        client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                        status = client->ParseRequest();
                    }
                } else {
                    int error = GetLastError();

                    if (!bytes || error != WSAEWOULDBLOCK) {
                        if (client->IsBusy()) {
                            if (bytes) {
                                LogError("Connection failed: %1", GetWin32ErrorString());
                            } else {
                                LogError("Connection closed unexpectedly");
                            }
                        }

                        ParkSocket(socket, bytes < 0 && error != WSAESHUTDOWN);
                        keep--;

                        continue;
                    }
                }
            }

            switch (status) {
                case http_RequestStatus::Busy: { /* Do nothing */ } break;

                case http_RequestStatus::Ready: {
                    socket->poll = false;
                    socket->handled = false;

                    async.Run([socket, this] {
                        http_IO *client = &socket->client;
                        http_RequestStatus status;

                        do {
                            daemon->RunHandler(client);

                            if (!client->Rearm(GetMonotonicClock())) {
                                status = http_RequestStatus::Shutdown;
                                break;
                            }

                            status = client->ParseRequest();
                        } while (status == http_RequestStatus::Ready);

                        bool done = (status == http_RequestStatus::Shutdown);
                        Wake(socket, done);

                        return true;
                    });
                } break;

                case http_RequestStatus::Shutdown: {
                    client->StartLinger();
                    shutdown((SOCKET)socket->sock, SD_SEND);
                } break;
            }

            int delay = (int)(client->timeout_at.load(std::memory_order_relaxed) - clock);

            if (delay <= 0) {
                int linger = client->StartLinger();

                if (!linger) {
                    shutdown((SOCKET)socket->sock, SD_BOTH);
                    delay = daemon->linger_timeout;
                } else if (socket->handled) {
                    ParkSocket(socket, true);
                    keep--;

                    continue;
                } else {
                    // Will leak once the handler finishes
                    if (linger < 0)
                        continue;

                    LogWarning("Handler for '%1' seems stuck", client->addr);
                    client->ExtendTimeout(1000);
                    delay = 1000;
                }
            }

            timeout = std::min(timeout, (unsigned int)delay);
        }
        sockets.len = keep;

        pfd_objs.RemoveFrom(2);
        pfd_sockets.RemoveFrom(2);

        // Prepare poll descriptors
        for (http_Socket *socket: sockets) {
            if (!socket->poll)
                continue;

            struct pollfd pfd = { (SOCKET)socket->sock, POLLIN, 0 };

            pfd_objs.Append(pfd);
            pfd_sockets.Append(socket);
        }

        // The timeout is unsigned to make it easier to use with std::min() without dealing
        // with the default value -1. If it stays at UINT_MAX, the (int) cast results in -1.
        int ready = WSAPoll(pfd_objs.ptr, (unsigned long)pfd_objs.len, (int)timeout);

        if (ready < 0) {
            LogError("Failed to poll descriptors: %1", GetWin32ErrorString());
            return;
        }
    }

    K_UNREACHABLE();
}

void http_Dispatcher::Wake(http_Socket *socket, bool done)
{
    uintptr_t addr = (uintptr_t)socket | (done ? 1 : 0);
    Size ret = send((SOCKET)pair_fd[1], (char *)&addr, K_SIZE(addr), 0);
    (void)ret;
}

http_Socket *http_Dispatcher::InitSocket(SOCKET sock, int64_t start, struct sockaddr *sa)
{
    http_Socket *socket = free_sockets.len ? free_sockets.data[--free_sockets.len] : new http_Socket(daemon, dispatcher);
    K_DEFER_N(err_guard) { delete socket; };

    socket->sock = (int)sock;
    socket->process = true;
    socket->poll = true;
    socket->handled = true;
    socket->nodelay = false;
    socket->lingered = 0;

    if (!socket->read_ev) {
        K_ASSERT(!socket->write_ev);

        socket->read_ev = CreateEvent(nullptr, true, false, nullptr);
        socket->write_ev = CreateEvent(nullptr, true, false, nullptr);

        if (!socket->read_ev || !socket->write_ev) [[unlikely]] {
            LogError("Failed to create event HANDLE: %1", GetWin32ErrorString());
            return nullptr;
        }
    }

    if (!socket->client.Init(socket, start, sa)) [[unlikely]]
        return nullptr;

    err_guard.Disable();
    return socket;
}

void http_Dispatcher::ParkSocket(http_Socket *socket, bool hard)
{
    if (socket->client.HasLeaked()) {
        socket->sock = -1;
    }

    if (hard) {
        struct linger sl = { 1, 0 };
        setsockopt(socket->sock, SOL_SOCKET, SO_LINGER, (char *)&sl, sizeof(sl));
    }

    if (free_sockets.Available()) {
        CloseSocket(socket->sock);
        socket->sock = -1;

        socket->client.socket = nullptr;
        socket->client.Rearm(-1);

        free_sockets.Append(socket);
    } else {
        delete socket;
    }
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    int error = 0;

    if (len < 0) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);

        BY_HANDLE_FILE_INFORMATION attr;
        if (!GetFileInformationByHandle(h, &attr)) {
            LogError("Cannot get file size: %1", GetWin32ErrorString());

            keepalive = false;
            return;
        }
        if (attr.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) {
            LogError("Cannot send non-regular file");

            keepalive = false;
            return;
        }


        len = (int64_t)(((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow);
    }

    if (!StartResponse()) [[unlikely]]
        return;

    // Send intro and file in one go
    Span<const char> intro = PrepareResponse(status, CompressionType::None, len, false);

    if (intro.len >= MaxSend || request.head || !len) {
        if (!daemon->WriteSocket(socket, intro.As<uint8_t>())) {
            keepalive = false;
            return;
        }

        if (request.head || !len)
            return;

        intro.len = 0;
    }

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    int64_t offset = 0;
    int64_t remain = len;

    // Send intro and start of file
    {
        TRANSMIT_FILE_BUFFERS tbuf = { (void *)intro.ptr, (DWORD)intro.len, nullptr, 0 };
        DWORD send = (DWORD)std::min(remain, (int64_t)MaxSend - intro.len);

        // Extend beforehand to prevent premature timeout
        int64_t bytes = intro.len + send;
        ExtendTimeout(bytes / 4, daemon->max_timeout);

BEGIN_IGNORE_MISSING_FIELDS
        OVERLAPPED ov = { .hEvent = socket->write_ev };
END_IGNORE_MISSING_FIELDS

        if (!TransmitFile((SOCKET)socket->sock, h, send, 0, &ov, &tbuf, 0)) {
            error = GetLastError();

            if (error != WSA_IO_PENDING)
                goto error;

            // Don't care but WSAGetOverlappedResult wants those
            DWORD sent;
            DWORD flags;

            if (!WSAGetOverlappedResult((SOCKET)socket->sock, &ov, &sent, true, &flags)) {
                error = GetLastError();
                goto error;
            }
        }

        offset += send;
        remain -= send;
    }

    // Send remaining file content
    while (remain) {
BEGIN_IGNORE_MISSING_FIELDS
        OVERLAPPED ov = {
            .Offset = (DWORD)(offset & 0xFFFFFFFFu),
            .OffsetHigh = (DWORD)(offset >> 32),
            .hEvent = socket->write_ev
        };
END_IGNORE_MISSING_FIELDS

        DWORD send = (DWORD)std::min(remain, (int64_t)MaxSend);

        // Extend beforehand to prevent premature timeout
        ExtendTimeout(send / 4, daemon->max_timeout);

        if (!TransmitFile((SOCKET)socket->sock, h, send, 0, &ov, nullptr, 0)) {
            error = GetLastError();

            if (error != WSA_IO_PENDING)
                goto error;

            // Don't care but WSAGetOverlappedResult wants those
            DWORD sent;
            DWORD flags;

            if (!WSAGetOverlappedResult((SOCKET)socket->sock, &ov, &sent, true, &flags)) {
                error = GetLastError();
                goto error;
            }
        }

        offset += send;
        remain -= send;
    }

    return;

error:
    if (error != WSAENOTCONN && error != WSAECONNRESET && error != WSAESHUTDOWN && error != WSA_OPERATION_ABORTED) {
        LogError("Failed to send file: %1", GetWin32ErrorString(error));
    }

    keepalive = false;
}

}

#endif
