// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <shared_mutex>
#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"
#include "../../libcc/libcc.hh"
#include "config.hh"
#include "structure.hh"
#include "thop.hh"
#include "user.hh"

namespace RG {

static const int64_t PruneDelay = 60 * 60000;

static const int64_t MaxSessionDelay = 1440 * 60000;
static const int64_t MaxKeyDelay = 120 * 60000;
static const int64_t RegenerateDelay = 15 * 60000;

struct Session {
    char session_key[129];
    char client_addr[65];
    char user_agent[134];

    const User *user;

    int64_t login_time;
    int64_t register_time;

    RG_HASH_TABLE_HANDLER_T(Session, const char *, session_key);
};

static std::shared_mutex sessions_mutex;
static HashTable<const char *, Session> sessions;

static Span<const char> SplitListValue(Span<const char> str,
                                       Span<const char> *out_remain, bool *out_enable)
{
    Span<const char> part = TrimStr(SplitStrAny(str, " ,", out_remain));

    if (out_enable) {
        if (part.len && part[0] == '+') {
            part = TrimStrLeft(part.Take(1, part.len - 1));
            *out_enable = true;
        } else if (part.len && part[0] == '-') {
            part = TrimStrLeft(part.Take(1, part.len - 1));
            *out_enable = false;
        } else {
            *out_enable = true;
        }
    }
    if (!part.len) {
        LogError("Ignoring empty list value");
    }

    return part;
}

bool UserSetBuilder::LoadIni(StreamReader &st)
{
    RG_DEFER_NC(out_guard, len = set.users.len) { set.users.RemoveFrom(len); };

    IniParser ini(&st);
    ini.PushLogHandler();
    RG_DEFER { PopLogHandler(); };

    bool valid = true;
    {
        HeapArray<const char *> allow(&allow_alloc);
        HeapArray<const char *> deny(&deny_alloc);

        bool warn_about_plain_passwords = true;

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            // TODO: Check validity, or maybe the INI parser checks are enough?
            const char *name = DuplicateString(prop.section, &set.str_alloc).ptr;
            User user = {};
            UnitRuleSet rule_set = {};

            bool first_property = true;
            do {
                if (prop.key == "Template") {
                    if (first_property) {
                        Size template_idx = map.FindValue(prop.value.ptr, -1);
                        if (template_idx >= 0) {
                            user = set.users[template_idx];
                            rule_set = rule_sets[template_idx];
                            allow.Append(rule_sets[template_idx].allow);
                            deny.Append(rule_sets[template_idx].deny);
                        } else {
                            LogError("Cannot copy from non-existent user '%1'", prop.value);
                            valid = false;
                        }
                    } else {
                        LogError("Template must be the first property");
                        valid = false;
                    }
                } else if (prop.key == "PasswordHash") {
                    user.password_hash = DuplicateString(prop.value, &set.str_alloc).ptr;
                } else if (prop.key == "Password") {
                    if (warn_about_plain_passwords) {
                        LogError("Plain passwords are not recommended, prefer PasswordHash");
                        warn_about_plain_passwords = false;
                    }

                    if (char hash[crypto_pwhash_STRBYTES];
                            crypto_pwhash_str(hash, prop.value.ptr, prop.value.len,
                                              crypto_pwhash_OPSLIMIT_MIN,
                                              crypto_pwhash_MEMLIMIT_MIN) == 0) {
                        user.password_hash = DuplicateString(hash, &set.str_alloc).ptr;
                    } else {
                        LogError("Failed to hash password");
                        valid = false;
                    }
                } else if (prop.key == "Permissions") {
                    while (prop.value.len) {
                        bool enable;
                        Span<const char> part = SplitListValue(prop.value, &prop.value, &enable);

                        if (part == "All") {
                            user.permissions = enable ? UINT_MAX : 0;
                        } else if (part.len) {
                            const char *const *name = FindIf(UserPermissionNames,
                                                             [&](const char *str) { return TestStr(str, part); });
                            if (name) {
                                user.permissions =
                                    ApplyMask(user.permissions, 1u << (name - UserPermissionNames), enable);
                            } else {
                                LogError("Unknown permission '%1'", part);
                                valid = false;
                            }
                        }
                    }
                } else if (prop.key == "McoDefault") {
                    if (prop.value == "Allow") {
                        rule_set.allow_default = true;
                    } else if (prop.value == "Deny") {
                        rule_set.allow_default = false;
                    } else {
                        LogError("Incorrect value '%1' for Default attribute", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "McoAllow") {
                    allow.Append(DuplicateString(prop.value, &set.str_alloc).ptr);
                } else if (prop.key == "McoDeny") {
                    deny.Append(DuplicateString(prop.value, &set.str_alloc).ptr);
                } else if (prop.key == "McoDispenseModes") {
                    while (prop.value.len) {
                        bool enable;
                        Span<const char> part = SplitListValue(prop.value, &prop.value, &enable);

                        if (part == "All") {
                            user.mco_dispense_modes = enable ? UINT_MAX : 0;
                        } else if (part.len) {
                            const OptionDesc *desc = FindIf(mco_DispenseModeOptions,
                                                            [&](const OptionDesc &desc) { return TestStr(desc.name, part); });
                            if (desc) {
                                user.mco_dispense_modes =
                                    ApplyMask(user.mco_dispense_modes, 1u << (desc - mco_DispenseModeOptions), enable);
                            } else {
                                LogError("Unknown dispensation mode '%1'", part);
                                valid = false;
                            }
                        }
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }

                first_property = false;
            } while (ini.NextInSection(&prop));

            user.name = name;
            rule_set.allow = allow.TrimAndLeak();
            rule_set.deny = deny.TrimAndLeak();

            if (map.Append(user.name, set.users.len).second) {
                set.users.Append(user);
                rule_sets.Append(rule_set);
            } else {
                LogError("Duplicate user '%1'", user.name);
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

bool UserSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (UserSetBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &UserSetBuilder::LoadIni;
        } else {
            LogError("Cannot load users from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
    }

    return success;
}

void UserSetBuilder::Finish(const StructureSet &structure_set, UserSet *out_set)
{
    RG_ASSERT(set.users.len == rule_sets.len);

    for (Size i = 0; i < set.users.len; i++) {
        User &user = set.users[i];
        const UnitRuleSet &rule_set = rule_sets[i];

        for (const Structure &structure: structure_set.structures) {
            for (const StructureEntity &ent: structure.entities) {
                if (CheckUnitPermission(rule_set, ent))
                    user.mco_allowed_units.Append(ent.unit);
            }
        }

        set.map.Append(&user);
    }

    SwapMemory(out_set, &set, RG_SIZE(set));
}

bool UserSetBuilder::CheckUnitPermission(const UnitRuleSet &rule_set, const StructureEntity &ent)
{
    const auto check_needle = [&](const char *needle) { return !!strstr(ent.path, needle); };

    if (rule_set.allow_default) {
        return !std::any_of(rule_set.deny.begin(), rule_set.deny.end(), check_needle) ||
               std::any_of(rule_set.allow.begin(), rule_set.allow.end(), check_needle);
    } else {
        return std::any_of(rule_set.allow.begin(), rule_set.allow.end(), check_needle) &&
               !std::any_of(rule_set.deny.begin(), rule_set.deny.end(), check_needle);
    }
}

bool LoadUserSet(Span<const char *const> filenames, const StructureSet &structure_set,
                 UserSet *out_set)
{
    UserSetBuilder user_set_builder;
    if (!user_set_builder.LoadFiles(filenames))
        return false;
    user_set_builder.Finish(structure_set, out_set);

    return true;
}

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

static Session *RegisterNewSession(const char *address, const char *user_agent, const User *user)
{
    std::unique_lock<std::shared_mutex> lock(sessions_mutex, std::defer_lock);

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

        lock.lock();

        std::pair<Session *, bool> ret = sessions.AppendDefault(session_key);
        if (RG_LIKELY(ret.second)) {
            session = ret.first;
            strcpy(session->session_key, session_key);
            break;
        }
    }

    // Fill in remaining information
    strcpy(session->client_addr, address);
    strncpy(session->user_agent, user_agent, RG_SIZE(session->user_agent) - 1);
    session->user = user;

    return session;
}

static void PruneStaleSessions()
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

    std::unique_lock<std::shared_mutex> lock(sessions_mutex);
    for (auto it = sessions.begin(); it != sessions.end(); it++) {
        const Session &session = *it;
        if (now - session.register_time >= MaxKeyDelay) {
            it.Remove();
        }
    }
}

// Call with sessions_mutex locked
static Session *FindSession(const http_RequestInfo &request, bool *out_mismatch = nullptr)
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
    const char *username = request.GetCookieValue("username");
    const char *user_agent = request.GetHeaderValue("User-Agent");
    if (!session_key || !username || !user_agent) {
        if (out_mismatch) {
            *out_mismatch = session_key || username;
        }
        return nullptr;
    }

    Session *session = sessions.Find(session_key);
    if (!session ||
            !TestStr(session->client_addr, address) ||
            !TestStr(session->user->name, username) ||
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

static void RegenerateSession(Session *session, http_IO *io)
{
    RG_ASSERT(session);

    int64_t now = GetMonotonicTime();

    if (now - session->register_time >= RegenerateDelay) {
        Session *new_session =
            RegisterNewSession(session->client_addr, session->user_agent, session->user);
        new_session->login_time = session->login_time;
        new_session->register_time = now;

        io->AddCookieHeader(thop_config.http.base_url, "session_key",
                            new_session->session_key, true);
    }
}

static void DeleteSessionCookies(http_IO *io)
{
    io->AddCookieHeader(thop_config.http.base_url, "session_key", nullptr);
    io->AddCookieHeader(thop_config.http.base_url, "url_key", nullptr);
    io->AddCookieHeader(thop_config.http.base_url, "username", nullptr);
}

const User *CheckSessionUser(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleSessions();

    Session *session;
    bool mismatch = false;
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex);
        session = FindSession(request, &mismatch);
    }

    if (session) {
        RegenerateSession(session, io);
        return session->user;
    } else if (mismatch) {
        DeleteSessionCookies(io);
        return nullptr;
    } else {
        return nullptr;
    }
}

void HandleLogin(const http_RequestInfo &request, const User *, http_IO *io)
{
    // Get POST and header values
    const char *username;
    const char *password;
    {
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        username = values.FindValue("username", nullptr);
        password = values.FindValue("password", nullptr);
        if (!username || !password) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
    }

    int64_t now = GetMonotonicTime();

    // Find and validate user
    const User *user = thop_user_set.FindUser(username);
    if (!user || !user->password_hash ||
            crypto_pwhash_str_verify(user->password_hash, password, strlen(password)) != 0) {
        int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
        WaitForDelay(safety_delay);

        LogError("Incorrect username or password");
        io->AttachError(403);
        return;
    }

    // Get main session security values
    char address[65];
    const char *user_agent;
    {
        RG_STATIC_ASSERT(RG_SIZE(address) == RG_SIZE(Session::client_addr));

        if (!GetClientAddress(request.conn, address)) {
            io->AttachError(422);
            return;
        }

        user_agent = request.GetHeaderValue("User-Agent");
        if (!user_agent) {
            LogError("Missing User-Agent header");
            io->AttachError(422);
            return;
        }
    }

    // Destroy current session (if any)
    if (const char *session_key = request.GetCookieValue("session_key"); session_key) {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex);
        sessions.Remove(session_key);
    }

    // Register session
    Session *session = RegisterNewSession(address, user_agent, user);
    session->login_time = now;
    session->register_time = now;

    // Create URL key
    char url_key[33];
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(url_key, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }

    // Build empty page
    http_JsonPageBuilder json(request.compression_type);
    json.Null();
    json.Finish(io);

    // Set session cookies
    io->AddCookieHeader(thop_config.http.base_url, "session_key", session->session_key, true);
    io->AddCookieHeader(thop_config.http.base_url, "url_key", url_key, false);
    io->AddCookieHeader(thop_config.http.base_url, "username", user->name, false);
}

void HandleLogout(const http_RequestInfo &request, const User *, http_IO *io)
{
    // Drop session
    {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex);
        sessions.Remove(FindSession(request));
    }

    http_JsonPageBuilder json(request.compression_type);
    json.Null();
    json.Finish(io);

    // Delete session cookies
    DeleteSessionCookies(io);
}

}
