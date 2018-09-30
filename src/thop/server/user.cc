// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../lib/libsodium/src/libsodium/include/sodium.h"

#include <shared_mutex>
#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

#include "thop.hh"

static const int64_t PruneDelay = 20 * 60 * 1000;
static const int64_t IdleSessionDelay = 4 * 3600 * 1000;

struct Session {
    char session_key[129];
    char client_addr[65];
    char user_agent[134];
    std::atomic_uint64_t last_seen;

    const User *user;

    HASH_TABLE_HANDLER_T(Session, const char *, session_key);
};

static std::shared_mutex sessions_mutex;
static HashTable<const char *, Session> sessions;

static bool GetClientAddress(MHD_Connection *conn, Span<char> out_address)
{
    int family;
    void *addr;
    {
        sockaddr *saddr =
            MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;

        family = saddr->sa_family;
        switch (saddr->sa_family) {
            case AF_INET: { addr = &((sockaddr_in *)saddr)->sin_addr; } break;
            case AF_INET6: { addr = &((sockaddr_in6 *)saddr)->sin6_addr; } break;
            default: { Assert(false); } break;
        }
    }

    if (!inet_ntop(family, addr, out_address.ptr, out_address.len)) {
        LogError("Cannot convert network address to text");
        return false;
    }

    return true;
}

static void PruneStaleSessions()
{
    uint64_t now = GetMonotonicTime();

    static std::mutex last_pruning_mutex;
    static uint64_t last_pruning = 0;
    {
        std::lock_guard<std::mutex> lock(last_pruning_mutex);
        if (now - last_pruning < PruneDelay)
            return;
        last_pruning = now;
    }

    std::unique_lock<std::shared_mutex> lock(sessions_mutex);
    for (auto it = sessions.begin(); it != sessions.end(); it++) {
        const Session &session = *it;
        if (now - session.last_seen >= IdleSessionDelay) {
            it.Remove();
        }
    }
}

// Call with sessions_mutex locked
static Session *FindSession(MHD_Connection *conn)
{
    uint64_t now = GetMonotonicTime();

    char address[65];
    if (!GetClientAddress(conn, address))
        return nullptr;

    const char *session_key = MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, "session_key");
    const char *username = MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, "username");
    const char *user_agent = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "User-Agent");
    if (!session_key || !username || !user_agent)
        return nullptr;

    Session *session = sessions.Find(session_key);
    if (!session)
        return nullptr;
    if (!TestStr(session->client_addr, address))
        return nullptr;
    if (!TestStr(session->user->name, username))
        return nullptr;
    if (strncmp(session->user_agent, user_agent, SIZE(session->user_agent) - 1))
        return nullptr;
    if (now - session->last_seen > IdleSessionDelay)
        return nullptr;

    session->last_seen = now;
    return session;
}

static void DeleteSessionCookies(MHD_Response *response)
{
    AddCookieHeader(response, "session_key", nullptr, 0, true);
    AddCookieHeader(response, "url_key", nullptr, 0, false);
    AddCookieHeader(response, "username", nullptr, 0, false);
}

const User *CheckSessionUser(MHD_Connection *conn)
{
    PruneStaleSessions();

    std::shared_lock<std::shared_mutex> lock(sessions_mutex);
    Session *session = FindSession(conn);

    return session ? session->user : nullptr;
}

void AddSessionHeaders(MHD_Connection *conn, const User *user, MHD_Response *response)
{
    if (!user && MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, "session_key")) {
        DeleteSessionCookies(response);
    }
}

int HandleConnect(const ConnectionInfo *conn, const char *, Response *out_response)
{
    char address[65];
    if (!GetClientAddress(conn->conn, address))
        return CreateErrorPage(500, out_response);

    const char *username = conn->post.FindValue("username", nullptr).ptr;
    const char *password = conn->post.FindValue("password", nullptr).ptr;
    const char *user_agent = MHD_lookup_connection_value(conn->conn, MHD_HEADER_KIND, "User-Agent");
    if (!username || !password || !user_agent)
        return CreateErrorPage(422, out_response);

    const User *user = thop_user_set.FindUser(username);
    if (!user || !user->password_hash ||
            crypto_pwhash_str_verify(user->password_hash, password, strlen(password)) != 0)
        return CreateErrorPage(404, out_response);

    // Create session key
    char session_key[129];
    {
        uint64_t buf[8];
        randombytes_buf(buf, SIZE(buf));
        Fmt(session_key, "%1%2%3%4%5%6%7%8",
            FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16),
            FmtHex(buf[2]).Pad0(-16), FmtHex(buf[3]).Pad0(-16),
            FmtHex(buf[4]).Pad0(-16), FmtHex(buf[5]).Pad0(-16),
            FmtHex(buf[6]).Pad0(-16), FmtHex(buf[7]).Pad0(-16));
    }

    // Create URL key
    char url_key[33];
    {
        uint64_t buf[2];
        randombytes_buf(&buf, SIZE(buf));
        Fmt(url_key, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Register session
    {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex);
        sessions.Remove(FindSession(conn->conn));

        // std::atomic objects are not copyable so we can't use Append()
        Session *session;
        {
            std::pair<Session *, bool> ret = sessions.AppendUninitialized(session_key);
            if (!ret.second) {
                LogError("Generated duplicate session key");
                return CreateErrorPage(500, out_response);
            }
            session = ret.first;
        }

        StaticAssert(SIZE(session->session_key) == SIZE(session_key));
        StaticAssert(SIZE(session->client_addr) == SIZE(address));
        strcpy(session->session_key, session_key);
        strcpy(session->client_addr, address);
        strncpy(session->user_agent, user_agent, SIZE(session->user_agent) - 1);
        session->last_seen = GetMonotonicTime();
        session->user = user;
    }

    MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    out_response->response.reset(response);

    // Set session cookies
    AddCookieHeader(response, "session_key", session_key, IdleSessionDelay / 1000, true);
    AddCookieHeader(response, "url_key", url_key, IdleSessionDelay / 1000, false);
    AddCookieHeader(response, "username", user->name, IdleSessionDelay / 1000, false);

    return 200;
}

int HandleDisconnect(const ConnectionInfo *conn, const char *, Response *out_response)
{
    // Drop session
    {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex);
        sessions.Remove(FindSession(conn->conn));
    }

    MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    out_response->response.reset(response);

    // Delete session cookies
    DeleteSessionCookies(response);

    return 200;
}
