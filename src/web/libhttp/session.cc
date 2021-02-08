// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "session.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int64_t PruneDelay = 60 * 60000;
static const int64_t MaxSessionDelay = 1440 * 60000;
static const int64_t MaxKeyDelay = 15 * 60000;
static const int64_t MaxLockDelay = 120 * 60000;
static const int64_t RegenerateDelay = 5 * 60000;

void http_SessionManager::Open2(const http_RequestInfo &request, http_IO *io, RetainPtr<RetainObject> udata)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    Session *session = CreateSession(request, io);
    if (!session)
        return;
    int64_t now = GetMonotonicTime();

    session->login_time = now;
    session->register_time = now;
    session->lock_time = now;
    session->udata = udata;
}

http_SessionManager::Session *
    http_SessionManager::CreateSession(const http_RequestInfo &request, http_IO *io)
{
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

        std::pair<Session *, bool> ret = sessions.TrySetDefault(session_key);

        if (RG_LIKELY(ret.second)) {
            session = ret.first;
            CopyString(session_key, session->session_key);
            break;
        }
    }

    // Create public randomized key (for use in session-specific URLs)
    {
        RG_STATIC_ASSERT(RG_SIZE(session->session_rnd) == 33);

        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(session->session_rnd, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Fill extra security values
    CopyString(user_agent, session->user_agent);

    // Set session cookies
    io->AddCookieHeader(cookie_path, "session_key", session->session_key, true);
    io->AddCookieHeader(cookie_path, "session_rnd", session->session_rnd, false);

    return session;
}

void http_SessionManager::Close(const http_RequestInfo &request, http_IO *io)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    sessions.Remove(FindSession(request));
    DeleteSessionCookies(request, io);
}

RetainObject *http_SessionManager::Find2(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleSessions();

    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    bool mismatch = false;
    bool locked = false;
    Session *session = FindSession(request, &mismatch, &locked);

    if (session) {
        RetainObject *udata = session->udata.GetRaw();
        int64_t now = GetMonotonicTime();

        // Regenerate session if needed
        if (now - session->register_time >= RegenerateDelay) {
            int64_t login_time = session->login_time;
            int64_t lock_time = session->lock_time;

            lock_shr.unlock();

            Session *session = CreateSession(request, io);

            if (session) {
                session->login_time = login_time;
                session->register_time = now;
                session->lock_time = locked ? lock_time : now;
                session->udata = udata;
            } else {
                DeleteSessionCookies(request, io);
            }
        }

        if (!locked) {
            udata->Ref();
            return udata;
        } else {
            return nullptr;
        }
    } else if (mismatch) {
        DeleteSessionCookies(request, io);
        return nullptr;
    } else {
        return nullptr;
    }
}

http_SessionManager::Session *
    http_SessionManager::FindSession(const http_RequestInfo &request,
                                     bool *out_mismatch, bool *out_locked)
{
    int64_t now = GetMonotonicTime();

    const char *session_key = request.GetCookieValue("session_key");
    const char *session_rnd = request.GetCookieValue("session_rnd");
    const char *user_agent = request.GetHeaderValue("User-Agent");
    if (!session_key || !user_agent) {
        if (out_mismatch) {
            *out_mismatch = session_key;
        }
        return nullptr;
    }

    // Until 2020-08-20 there was an IP check below, but it caused problems with mobile
    // connectivity and with dual-stack browsers. For example, on occasion, I would get
    // disconnected during localhost tests because login used IPv4 and a subsequent request
    // used IPv6, or vice versa.
    Session *session = sessions.Find(session_key);
    if (!session ||
            (session_rnd && !TestStr(session->session_rnd, session_rnd)) ||
#ifdef NDEBUG
            strncmp(session->user_agent, user_agent, RG_SIZE(session->user_agent) - 1) != 0 ||
#endif
            now - session->login_time >= MaxSessionDelay ||
            now - session->register_time >= MaxKeyDelay ||
            now - session->lock_time >= MaxLockDelay) {
        if (out_mismatch) {
            *out_mismatch = true;
        }
        return nullptr;
    }

    if (out_mismatch) {
        *out_mismatch = false;
    }
    if (out_locked) {
        *out_locked = !session_rnd;
    }
    return session;
}

void http_SessionManager::DeleteSessionCookies(const http_RequestInfo &request, http_IO *io)
{
    io->AddCookieHeader(cookie_path, "session_key", nullptr, true);
    io->AddCookieHeader(cookie_path, "session_rnd", nullptr, false);
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

    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    for (auto it = sessions.begin(); it != sessions.end(); it++) {
        const Session &session = *it;
        if (now - session.register_time >= MaxKeyDelay) {
            it.Remove();
        }
    }
}

}
