// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "session.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

namespace RG {

static const int64_t PruneDelay = 60 * 60000;
static const int64_t MaxSessionDelay = 1440 * 60000;
static const int64_t MaxKeyDelay = 120 * 60000;
static const int64_t RegenerateDelay = 15 * 60000;

static bool GetClientAddress(MHD_Connection *conn, Span<char> out_address)
{
    RG_ASSERT(out_address.len);

    int family;
    void *addr;
    {
        sockaddr *saddr =
            MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;

        family = saddr->sa_family;
        switch (saddr->sa_family) {
            case AF_INET: { addr = &((sockaddr_in *)saddr)->sin_addr; } break;
            case AF_INET6: { addr = &((sockaddr_in6 *)saddr)->sin6_addr; } break;
            default: { RG_ASSERT(false); } break;
        }
    }

    if (!inet_ntop(family, addr, out_address.ptr, out_address.len)) {
        LogError("Cannot convert network address to text");
        return false;
    }

    return true;
}

void http_SessionManager::Open2(const http_RequestInfo &request, http_IO *io,
                                std::shared_ptr<void> udata)
{
    std::unique_lock<std::shared_mutex> lock(mutex);

    Session *session = CreateSession(request, io);
    if (!session)
        return;
    int64_t now = GetMonotonicTime();

    session->login_time = now;
    session->register_time = now;
    session->udata = udata;
}

http_SessionManager::Session *
    http_SessionManager::CreateSession(const http_RequestInfo &request, http_IO *io)
{
    char address[65];
    RG_STATIC_ASSERT(RG_SIZE(address) == RG_SIZE(Session::client_addr));
    if (!GetClientAddress(request.conn, address)) {
        io->AttachError(422);
        return nullptr;
    }

    const char *user_agent = request.GetHeaderValue("User-Agent");
    if (!user_agent) {
        LogError("Missing User-Agent header");
        io->AttachError(422);
        return nullptr;
    }

    // Register session with unique key
    Session *session;
    for (;;) {
        char session_key[129];
        {
            RG_STATIC_ASSERT(RG_SIZE(session_key) == RG_SIZE(Session::session_key));

            uint64_t buf[8];
            randombytes_buf(buf, RG_SIZE(buf));
            Fmt(session_key, "%1%2%3%4%5%6%7%8",
                FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16),
                FmtHex(buf[2]).Pad0(-16), FmtHex(buf[3]).Pad0(-16),
                FmtHex(buf[4]).Pad0(-16), FmtHex(buf[5]).Pad0(-16),
                FmtHex(buf[6]).Pad0(-16), FmtHex(buf[7]).Pad0(-16));
        }

        std::pair<Session *, bool> ret = sessions.AppendDefault(session_key);

        if (RG_LIKELY(ret.second)) {
            session = ret.first;
            strcpy(session->session_key, session_key);
            break;
        }
    }

    // Create public randomized key (for use in session-specific URLs)
    char session_rnd[33];
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(session_rnd, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Fill extra security values
    strcpy(session->client_addr, address);
    strncpy(session->user_agent, user_agent, RG_SIZE(session->user_agent) - 1);

    // Set session cookies
    io->AddCookieHeader(request.base_url, "session_key", session->session_key, true);
    io->AddCookieHeader(request.base_url, "session_rnd", session_rnd, false);

    return session;
}

static void DeleteSessionCookies(const http_RequestInfo &request, http_IO *io)
{
    io->AddCookieHeader(request.base_url, "session_key", nullptr);
    io->AddCookieHeader(request.base_url, "session_rnd", nullptr);
}

void http_SessionManager::Close(const http_RequestInfo &request, http_IO *io)
{
    std::unique_lock<std::shared_mutex> lock(mutex);

    sessions.Remove(FindSession(request));
    DeleteSessionCookies(request, io);
}

std::shared_ptr<void> http_SessionManager::Find2(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleSessions();

    std::shared_lock<std::shared_mutex> lock(mutex);

    bool mismatch = false;
    Session *session = FindSession(request, &mismatch);

    if (session) {
        std::shared_ptr<void> udata = session->udata;
        int64_t now = GetMonotonicTime();

        // Regenerate session if needed
        if (now - session->register_time >= RegenerateDelay) {
            int64_t login_time = session->login_time;
            std::shared_ptr<void> udata = session->udata;

            lock.unlock();

            Session *session = CreateSession(request, io);

            if (session) {
                session->login_time = login_time;
                session->register_time = now;
                session->udata = udata;
            } else {
                DeleteSessionCookies(request, io);
            }
        }

        return udata;
    } else if (mismatch) {
        DeleteSessionCookies(request, io);
        return nullptr;
    } else {
        return nullptr;
    }
}

http_SessionManager::Session *
    http_SessionManager::FindSession(const http_RequestInfo &request, bool *out_mismatch)
{
    int64_t now = GetMonotonicTime();

    char address[65];
    if (!GetClientAddress(request.conn, address)) {
        if (out_mismatch) {
            *out_mismatch = false;
        }
        return nullptr;
    }

    const char *session_key = request.GetCookieValue("session_key");
    const char *user_agent = request.GetHeaderValue("User-Agent");
    if (!session_key || !user_agent) {
        if (out_mismatch) {
            *out_mismatch = session_key;
        }
        return nullptr;
    }

    Session *session = sessions.Find(session_key);
    if (!session ||
            !TestStr(session->client_addr, address) ||
            strncmp(session->user_agent, user_agent, RG_SIZE(session->user_agent) - 1) ||
            now - session->login_time >= MaxSessionDelay ||
            now - session->register_time >= MaxKeyDelay) {
        if (out_mismatch) {
            *out_mismatch = true;
        }
        return nullptr;
    }

    if (out_mismatch) {
        *out_mismatch = false;
    }
    return session;
}

void http_SessionManager::PruneStaleSessions()
{
    int64_t now = GetMonotonicTime();

    static std::mutex last_pruning_mutex;
    static int64_t last_pruning = 0;
    {
        std::lock_guard<std::mutex> lock(last_pruning_mutex);
        if (now - last_pruning < PruneDelay)
            return;
        last_pruning = now;
    }

    std::unique_lock<std::shared_mutex> lock(mutex);

    for (auto it = sessions.begin(); it != sessions.end(); it++) {
        const Session &session = *it;
        if (now - session.register_time >= MaxKeyDelay) {
            it.Remove();
        }
    }
}

}
