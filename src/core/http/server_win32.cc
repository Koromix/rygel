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

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"

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

#if !defined(UNIX_PATH_MAX)
    #define UNIX_PATH_MAX 108
#endif
typedef struct sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[UNIX_PATH_MAX];
} SOCKADDR_UN, *PSOCKADDR_UN;

namespace RG {

static const Size AcceptReceiveLen = Kibibytes(4);
static const Size AcceptAddressLen = 2 * sizeof(SOCKADDR_STORAGE) + 16;

static const int ParallelConnections = 256;
static const int WorkersPerDispatcher = 4;

enum class PendingOperation {
    None,
    Accept,
    Disconnect,
    Read
};

struct SocketData {
    int sock = -1;
    bool connected = false;

    PendingOperation op = PendingOperation::None;
    OVERLAPPED overlapped = {};
    HeapArray<uint8_t> buf;

    std::unique_ptr<http_IO> client = nullptr;

    ~SocketData() { closesocket(sock); };
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

    HeapArray<SocketData *> sockets;
    HeapArray<SocketData *> free_sockets;

    LocalArray<http_IO *, 256> free_clients;

public:
    http_Dispatcher(http_Daemon *daemon, HANDLE iocp, IndirectFunctions fn) : daemon(daemon), iocp(iocp), fn(fn) {}
    ~http_Dispatcher();

    bool PostAccept();

    bool Run();

private:
    void ProcessClient(int64_t now, SocketData *socket, http_IO *client);

    bool PostRead(SocketData *socket);

    SocketData *InitSocket();
    void DisconnectSocket(SocketData *socket);
    void DestroySocket(SocketData *socket);

    http_IO *InitClient(int sock, int64_t start, struct sockaddr *sa);
    void ParkClient(http_IO *client);
};

static void SetSocketPush(int sock, bool push)
{
    int flag = push;
    setsockopt((SOCKET)sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    if (push) {
        send(sock, nullptr, 0, 0);
    }
}

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(listener < 0);

    if (!InitConfig(config))
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listener = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM | SOCK_OVERLAPPED); } break;
        case SocketType::Unix: { listener = OpenUnixSocket(config.unix_path, SOCK_STREAM | SOCK_OVERLAPPED); } break;
    }
    if (listener < 0)
        return false;

    if (listen(listener, 256) < 0) {
        errno = TranslateWinSockError();

        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

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

bool http_Daemon::Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(listener >= 0);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    RG_DEFER_N(err_guard) {
        if (async) {
            delete async;
            async = nullptr;
        }

        if (iocp) {
            CloseHandle(iocp);
            iocp = nullptr;
        }

        for (http_Dispatcher *dispatcher: dispatchers) {
            delete dispatcher;
        }
        dispatchers.Clear();
    };

    // Heuristic found on MSDN
    async = new Async(1 + 4 * GetCoreCount());

    iocp = CreateIoCompletionPort((HANDLE)(uintptr_t)listener, nullptr, 1, 0);
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

    for (Size i = 1; i < async->GetWorkerCount(); i++) {
        http_Dispatcher *dispatcher = new http_Dispatcher(this, iocp, fn);
        dispatchers.Append(dispatcher);
    }
    for (Size i = 0; i < ParallelConnections; i++) {
        http_Dispatcher *dispatcher = dispatchers[i % dispatchers.len];
        if (!dispatcher->PostAccept())
            return false;
    }

    // Cannot fail anymore
    err_guard.Disable();

    handle_func = func;

    for (http_Dispatcher *dispatcher: dispatchers) {
        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    for (Size i = 0; i < dispatchers.len; i++) {
        PostQueuedCompletionStatus(iocp, 0, 0, nullptr);
    }

    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    for (http_Dispatcher *dispatcher: dispatchers) {
        delete dispatcher;
    }
    dispatchers.Clear();

    CloseSocket(listener);
    listener = -1;

    if (iocp) {
        CloseHandle(iocp);
        iocp = nullptr;
    }

    handle_func = {};
}

static SocketData *SocketFromOverlapped(void *ptr)
{
    uint8_t *data = (uint8_t *)ptr;
    return (SocketData *)(data - offsetof(SocketData, overlapped));
}

http_Dispatcher::~http_Dispatcher()
{
    for (SocketData *socket: sockets) {
        delete socket;
    }
    for (http_IO *client: free_clients) {
        delete client;
    }

    sockets.Clear();
    free_sockets.Clear();
    free_clients.Clear();
}

bool http_Dispatcher::PostAccept()
{
    SocketData *socket = InitSocket();
    if (!socket)
        return false;
    RG_DEFER_N(err_guard) { DisconnectSocket(socket); };

    socket->buf.AppendDefault(AcceptReceiveLen + 2 * AcceptAddressLen);

    DWORD dummy = 0;

retry:
    if (!fn.AcceptEx((SOCKET)daemon->listener, (SOCKET)socket->sock, socket->buf.ptr, AcceptReceiveLen,
                     AcceptAddressLen, AcceptAddressLen, &dummy, &socket->overlapped) &&
            WSAGetLastError() != ERROR_IO_PENDING) {
        errno = TranslateWinSockError();

        if (errno == ECONNRESET)
            goto retry;

        LogError("Failed to issue socket accept operation: %1", strerror(errno));
        return false;
    }

    socket->op = PendingOperation::Accept;
    err_guard.Disable();

    return true;
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

        // Exit signal
        if (!key) [[unlikely]] {
            RG_ASSERT(success);
            return true;
        }

        int64_t now = GetMonotonicTime();
        SocketData *socket = SocketFromOverlapped(overlapped);
        PendingOperation op = socket->op;

        socket->op = PendingOperation::None;

        switch (op) {
            case PendingOperation::None: {} break;

            case PendingOperation::Accept: {
                PostAccept();

                if (!success) {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = true;
                setsockopt(socket->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&daemon->listener, RG_SIZE(daemon->listener));

                SetSocketPush(socket->sock, false);

                sockaddr *local_addr = nullptr;
                sockaddr *remote_addr = nullptr;
                int local_len;
                int remote_len;

                fn.GetAcceptExSockaddrs(socket->buf.ptr, AcceptReceiveLen, AcceptAddressLen, AcceptAddressLen,
                                        &local_addr, &local_len, &remote_addr, &remote_len);

                http_IO *client = InitClient(socket->sock, now, remote_addr);
                if (!client) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }
                socket->client.reset(client);

                socket->buf.len = (Size)transferred;
                std::swap(socket->buf, client->incoming.buf);

                ProcessClient(now, socket, client);
            } break;

            case PendingOperation::Disconnect: {
                if (!success) [[unlikely]] {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = false;
                free_sockets.Append(socket);
            } break;

            case PendingOperation::Read: {
                RG_ASSERT(socket->client);

                if (!success) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                http_IO *client = socket->client.get();

                client->incoming.buf.len += (Size)transferred;
                client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                ProcessClient(now, socket, client);
            } break;
        }
    }

    RG_UNREACHABLE();
}

void http_Dispatcher::ProcessClient(int64_t now, SocketData *socket, http_IO *client)
{
    RG_ASSERT(client == socket->client.get());

    http_IO::RequestStatus status = client->ProcessIncoming(now);

    switch (status) {
        case http_IO::RequestStatus::Incomplete: {
            if (!PostRead(socket)) {
                DisconnectSocket(socket);
            }
        } break;

        case http_IO::RequestStatus::Ready: {
            if (!client->InitAddress()) {
                client->request.keepalive = false;
                client->SendError(400);
                DisconnectSocket(socket);

                break;
            }

            client->request.keepalive &= (now < client->socket_start + daemon->keepalive_time);

            daemon->RunHandler(client);

            if (client->request.keepalive) {
                client->Rearm(now);

                if (!PostRead(socket)) {
                    DisconnectSocket(socket);
                }
            } else {
                DisconnectSocket(socket);
            }
        } break;

        case http_IO::RequestStatus::Busy: {} break;

        case http_IO::RequestStatus::Close: { DisconnectSocket(socket); } break;
    }
}

bool http_Dispatcher::PostRead(SocketData *socket)
{
    if (socket->op == PendingOperation::Read)
        return true;

    RG_ASSERT(socket->op == PendingOperation::None);
    RG_ASSERT(socket->client);

    http_IO *client = socket->client.get();

    client->incoming.buf.Grow(Mebibytes(1));

    WSABUF buf = { (unsigned long)client->incoming.buf.Available() - 1, (char *)client->incoming.buf.end() };
    DWORD received = 0;
    DWORD flags = 0;

    if (WSARecv((SOCKET)socket->sock, &buf, 1, &received, &flags, &socket->overlapped, nullptr) &&
            WSAGetLastError() != ERROR_IO_PENDING) {
        errno = TranslateWinSockError();

        if (errno != ECONNRESET) {
            LogError("Failed to read from socket: %1", strerror(errno));
        }

        return false;
    }

    socket->op = PendingOperation::Read;

    return true;
}

SocketData *http_Dispatcher::InitSocket()
{
    SocketData *socket = nullptr;

    if (free_sockets.len) {
        int idx = GetRandomInt(0, (int)free_sockets.len);

        socket = free_sockets[idx];

        std::swap(free_sockets[idx], free_sockets[free_sockets.len - 1]);
        free_sockets.len--;
    } else {
        socket = new SocketData();
        RG_DEFER_N(err_guard) { delete socket; };

        socket->sock = CreateSocket(daemon->sock_type, SOCK_STREAM | SOCK_OVERLAPPED);
        if (socket->sock < 0)
            return nullptr;

        if (!CreateIoCompletionPort((HANDLE)(uintptr_t)socket->sock, iocp, 1, 0)) {
            LogError("Failed to associate socket with IOCP: %1", GetWin32ErrorString());
            return nullptr;
        }

        err_guard.Disable();
        sockets.Append(socket);
    }

    return socket;
}

void http_Dispatcher::DisconnectSocket(SocketData *socket)
{
    RG_ASSERT(socket->op == PendingOperation::None);

    if (socket->client) {
        http_IO *client = socket->client.release();
        ParkClient(client);
    }

    socket->buf.RemoveFrom(0);

    if (socket->connected) {
        if (!fn.DisconnectEx((SOCKET)socket->sock, &socket->overlapped, TF_REUSE_SOCKET, 0) &&
                WSAGetLastError() != ERROR_IO_PENDING) {
            errno = TranslateWinSockError();

            LogError("Failed to reuse socket: %1", strerror(errno));
            DestroySocket(socket);
        }

        socket->op = PendingOperation::Disconnect;
    }
}

void http_Dispatcher::DestroySocket(SocketData *socket)
{
    // XXX: leak
    closesocket(socket->sock);
    socket->sock = -1;
}

http_IO *http_Dispatcher::InitClient(int sock, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (free_clients.len) {
        int idx = GetRandomInt(0, (int)free_clients.len);

        client = free_clients[idx];

        std::swap(free_clients[idx], free_clients[free_clients.len - 1]);
        free_clients.len--;
    } else {
        client = new http_IO(daemon);
    }

    if (!client->Init(sock, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Dispatcher::ParkClient(http_IO *client)
{
    if (free_clients.Available()) {
        client->Rearm(-1);
        free_clients.Append(client);
    } else {
        delete client;
    }
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    RG_DEFER {
        SetSocketPush(sock, true);
        response.sent = true;
    };

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    Span<const char> intro = PrepareResponse(status, encoding, len);
    writer.Write(intro);

    if (len >= 0) {
        if (encoding != CompressionType::None) {
            writer.Close();
            writer.Open(write, "<http>", encoding);
        }

        request.keepalive &= func(sock, &writer);
    } else {
        const auto chunk = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };
        StreamWriter chunker(chunk, "<http>", encoding);

        if (func(-1, &chunker)) {
            request.keepalive &= chunker.Close();
            writer.Write("0\r\n\r\n");
        } else {
            request.keepalive = false;
        }
    }

    request.keepalive &= writer.Close();
}

void http_IO::SendFile(int status, int fd, int64_t len)
{
    RG_ASSERT(!response.sent);

    RG_DEFER {
        SetSocketPush(sock, true);
        response.sent = true;
    };

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    int64_t offset = 0;

    Span<const char> intro = PrepareResponse(status, CompressionType::None, len);

    len += intro.len;

    // Send intro and file in one go
    {
        TRANSMIT_FILE_BUFFERS tbuf = { (void *)intro.ptr, (DWORD)intro.len, nullptr, 0 };
        DWORD send = (DWORD)std::min(len, (Size)UINT32_MAX);

        BOOL success = TransmitFile((SOCKET)sock, h, 0, 0, nullptr, &tbuf, 0);

        if (!success) [[unlikely]] {
            LogError("Failed to send file: %1", strerror(TranslateWinSockError()));
            return;
        }

        offset += send - intro.len;
        len -= (Size)send;
    }

    while (len) {
        if (!SetFilePointerEx(h, { .QuadPart = offset }, nullptr, FILE_BEGIN)) {
            LogError("Failed to send file: %1", GetWin32ErrorString());
            return;
        }

        DWORD send = (DWORD)std::min(len, (Size)UINT32_MAX);
        BOOL success = TransmitFile((SOCKET)sock, h, 0, 0, nullptr, nullptr, 0);

        if (!success) [[unlikely]] {
            LogError("Failed to send file: %1", strerror(TranslateWinSockError()));
            return;
        }

        offset += (Size)send;
        len -= (Size)send;
    }
}

http_IO::RequestStatus http_IO::ProcessIncoming(int64_t now)
{
    RG_ASSERT(sock >= 0);

    if (ready)
        return RequestStatus::Busy;

    bool complete = false;

    // Gather request line and headers
    while (incoming.buf.len - incoming.pos >= 4) {
        uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
        incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

        if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
            LogError("Excessive request size");
            SendError(413);
            return RequestStatus::Close;
        }

        const char *end = (const char *)incoming.buf.ptr + incoming.pos;

        if (end[0] == '\r' && end[1] == '\n' && end[2] == '\r' && end[3] == '\n') {
            incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
            incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 4, incoming.buf.len - incoming.pos - 4);

            complete = true;
            break;
        } else if (end[0] == '\n' && end[1] == '\n') {
            incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
            incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 2, incoming.buf.len - incoming.pos - 2);

            complete = true;
            break;
        }

        incoming.pos++;
    }

    if (!complete)
        return RequestStatus::Incomplete;
    if (!ParseRequest(incoming.intro))
        return RequestStatus::Close;

    ready = true;
    return RequestStatus::Ready;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
        int len = (int)std::min(data.len, (Size)INT_MAX);
        Size sent = send((SOCKET)sock, (const char *)data.ptr, len, 0);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            errno = TranslateWinSockError();

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        data.ptr += sent;
        data.len -= sent;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    while (data.len) {
        LocalArray<uint8_t, 16384> buf;

        Size copy_len = std::min(RG_SIZE(buf.data) - 8, data.len);

        buf.len = 8 + copy_len;
        Fmt(buf.As<char>(), "%1\r\n", FmtHex(copy_len).Pad0(-4));
        MemCpy(buf.data + 6, data.ptr, copy_len);
        buf.data[6 + copy_len + 0] = '\r';
        buf.data[6 + copy_len + 1] = '\n';

        Span<const uint8_t> remain = buf;

        do {
            int len = (int)std::min(remain.len, (Size)INT_MAX);
            Size sent = send((SOCKET)sock, (const char *)remain.ptr, len, 0);

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

                errno = TranslateWinSockError();

                if (errno != EPIPE && errno != ECONNRESET) {
                    LogError("Failed to send to client: %1", strerror(errno));
                }
                return false;
            }

            remain.ptr += sent;
            remain.len -= sent;
         } while (remain.len);

        data.ptr += copy_len;
        data.len -= copy_len;
    }

    return true;
}

}

#endif
