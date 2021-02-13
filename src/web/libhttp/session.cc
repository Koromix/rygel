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

    Session *session = sessions.AppendDefault();

    // Register session with unique key
    for (;;) {
        RG_STATIC_ASSERT(RG_SIZE(Session::session_key) == 65);

        {
            uint64_t buf[4];
            randombytes_buf(buf, RG_SIZE(buf));
            Fmt(session->session_key, "%1%2%3%4",
                FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16),
                FmtHex(buf[2]).Pad0(-16), FmtHex(buf[3]).Pad0(-16));
        }

        if (RG_LIKELY(sessions_map.TrySet(session).second))
            break;
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

    // We don't care about those but for performance reasons FindSession()
    // always writes those.
    bool mismatch;
    bool locked;
    Session **ptr = FindSession(request, &mismatch, &locked);

    sessions_map.Remove(ptr);
    DeleteSessionCookies(request, io);
}

RetainObject *http_SessionManager::Find2(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleSessions();

    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    bool mismatch = false;
    bool locked = false;
    Session **ptr = FindSession(request, &mismatch, &locked);

    if (ptr) {
        Session *session = *ptr;
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

http_SessionManager::Session **
    http_SessionManager::FindSession(const http_RequestInfo &request, bool *out_mismatch, bool *out_locked)
{
    int64_t now = GetMonotonicTime();

    const char *session_key = request.GetCookieValue("session_key");
    const char *session_rnd = request.GetCookieValue("session_rnd");
    const char *user_agent = request.GetHeaderValue("User-Agent");
    if (!session_key || !user_agent) {
        *out_mismatch = session_key;
        return nullptr;
    }

    Session **ptr = sessions_map.Find(session_key);
    if (!ptr) {
        *out_mismatch = true;
        return nullptr;
    }

    // Until 2020-08-20 there was an IP check below, but it caused problems with mobile
    // connectivity and with dual-stack browsers. For example, on occasion, I would get
    // disconnected during localhost tests because login used IPv4 and a subsequent request
    // used IPv6, or vice versa.
    Session *session = *ptr;
    if (now - session->login_time >= MaxSessionDelay ||
            now - session->register_time >= MaxKeyDelay ||
            now - session->lock_time >= MaxLockDelay ||
            (session_rnd && !TestStr(session->session_rnd, session_rnd)) ||
#ifdef NDEBUG
            strncmp(session->user_agent, user_agent, RG_SIZE(session->user_agent) - 1) != 0 ||
#endif
            (session_rnd && !TestStr(session->session_rnd, session_rnd))) {
        *out_mismatch = true;
        return nullptr;
    }

    *out_mismatch = false;
    *out_locked = !session_rnd;
    return ptr;
}

void http_SessionManager::DeleteSessionCookies(const http_RequestInfo &request, http_IO *io)
{
    io->AddCookieHeader(cookie_path, "session_key", nullptr, true);
    io->AddCookieHeader(cookie_path, "session_rnd", nullptr, false);
}

void http_SessionManager::PruneStaleSessions()
{
    static std::atomic_int64_t last_pruning {0};
    static std::mutex last_pruning_mutex;

    // Time to prune?
    int64_t now = GetMonotonicTime();
    if (now - last_pruning.load(std::memory_order_acquire) >= PruneDelay) {
        std::lock_guard<std::mutex> lock(last_pruning_mutex);
        if (now - last_pruning.load(std::memory_order_relaxed) < PruneDelay)
            return;
        last_pruning.store(now, std::memory_order_release);
    }

    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    Size expired = 0;
    for (const Session &session: sessions) {
        if (now - session.register_time < MaxKeyDelay)
            break;

        sessions_map.Remove(session.session_key);
        expired++;
    }

    sessions.RemoveFirst(expired);
    sessions_map.Trim();
}

}
