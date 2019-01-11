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
#include "structure.hh"
#include "user.hh"

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

bool UserSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.users.len) { set.users.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

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
                } else if (prop.key == "Default") {
                    if (prop.value == "Allow") {
                        rule_set.allow_default = true;
                    } else if (prop.value == "Deny") {
                        rule_set.allow_default = false;
                    } else {
                        LogError("Incorrect value '%1' for Default attribute", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "Allow") {
                    allow.Append(DuplicateString(prop.value, &set.str_alloc).ptr);
                } else if (prop.key == "Deny") {
                    deny.Append(DuplicateString(prop.value, &set.str_alloc).ptr);
                } else if (prop.key == "Permissions") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));
                        if (part.len) {
                            bool enable;
                            if (part[0] == '+') {
                                enable = true;
                                part = part.Take(1, part.len - 1);
                            } else if (part[0] == '-') {
                                enable = false;
                                part = part.Take(1, part.len - 1);
                            } else {
                                enable = true;
                            }

                            if (part == "All") {
                                user.permissions = enable ? UINT_MAX : 0;
                            } else {
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
                    }
                } else if (prop.key == "DispenseModes") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));
                        if (part.len) {
                            bool enable;
                            if (part[0] == '+') {
                                enable = true;
                                part = part.Take(1, part.len - 1);
                            } else if (part[0] == '-') {
                                enable = false;
                                part = part.Take(1, part.len - 1);
                            } else {
                                enable = true;
                            }

                            if (part == "All") {
                                user.dispense_modes = enable ? UINT_MAX : 0;
                            } else {
                                const OptionDesc *desc = FindIf(mco_DispenseModeOptions,
                                                                [&](const OptionDesc &desc) { return TestStr(desc.name, part); });
                                if (desc) {
                                    user.dispense_modes =
                                        ApplyMask(user.dispense_modes, 1u << (desc - mco_DispenseModeOptions), enable);
                                } else {
                                    LogError("Unknown dispensation mode '%1'", part);
                                    valid = false;
                                }
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
    if (ini.error || !valid)
        return false;

    out_guard.disable();
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
        if (st.error) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
    }

    return success;
}

void UserSetBuilder::Finish(const StructureSet &structure_set, mco_DispenseMode dispense_mode,
                            UserSet *out_set)
{
    DebugAssert(set.users.len == rule_sets.len);

    for (Size i = 0; i < set.users.len; i++) {
        User &user = set.users[i];
        const UnitRuleSet &rule_set = rule_sets[i];

        user.dispense_modes |= 1 << (int)dispense_mode;

        for (const Structure &structure: structure_set.structures) {
            for (const StructureEntity &ent: structure.entities) {
                if (CheckUnitPermission(rule_set, ent))
                    user.allowed_units.Append(ent.unit);
            }
        }

        set.map.Append(&user);
    }

    SwapMemory(out_set, &set, SIZE(set));
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
                 mco_DispenseMode dispense_mode, UserSet *out_set)
{
    UserSetBuilder user_set_builder;
    if (!user_set_builder.LoadFiles(filenames))
        return false;
    user_set_builder.Finish(structure_set, dispense_mode, out_set);

    return true;
}

static bool GetClientAddress(MHD_Connection *conn, Span<char> out_address)
{
    DebugAssert(out_address.len);

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
static Session *FindSession(MHD_Connection *conn, bool *out_mismatch = nullptr)
{
    uint64_t now = GetMonotonicTime();

    char address[65];
    if (!GetClientAddress(conn, address)) {
        if (out_mismatch) {
            *out_mismatch = false;
        }
        return nullptr;
    }

    const char *session_key = MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, "session_key");
    const char *username = MHD_lookup_connection_value(conn, MHD_COOKIE_KIND, "username");
    const char *user_agent = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "User-Agent");
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
            strncmp(session->user_agent, user_agent, SIZE(session->user_agent) - 1) ||
            now - session->last_seen > IdleSessionDelay) {
        if (out_mismatch) {
            *out_mismatch = true;
        }
        return nullptr;
    }

    // Avoid pruning (not idle)
    session->last_seen = now;

    if (out_mismatch) {
        *out_mismatch = false;
    }
    return session;
}

const User *CheckSessionUser(MHD_Connection *conn, bool *out_mismatch)
{
    PruneStaleSessions();

    std::shared_lock<std::shared_mutex> lock(sessions_mutex);
    Session *session = FindSession(conn, out_mismatch);

    return session ? session->user : nullptr;
}

void DeleteSessionCookies(MHD_Response *response)
{
    AddCookieHeader(response, "session_key", nullptr);
    AddCookieHeader(response, "url_key", nullptr);
    AddCookieHeader(response, "username", nullptr);
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

    // Find and validate user
    const User *user = thop_user_set.FindUser(username);
    if (!user || !user->password_hash ||
            crypto_pwhash_str_verify(user->password_hash, password, strlen(password)) != 0)
        return CreateErrorPage(403, out_response);

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

        // Drop current session (if any)
        sessions.Remove(FindSession(conn->conn));

        // std::atomic objects are not copyable so we can't use Append()
        Session *session;
        {
            std::pair<Session *, bool> ret = sessions.AppendDefault(session_key);
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
    AddCookieHeader(response, "session_key", session_key, true);
    AddCookieHeader(response, "url_key", url_key, false);
    AddCookieHeader(response, "username", user->name, false);

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
