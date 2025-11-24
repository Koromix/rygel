// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "structure.hh"
#include "thop.hh"
#include "user.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static http_SessionManager<const User> sessions;

class UserSetBuilder {
    K_DELETE_COPY(UserSetBuilder)

    struct UnitRuleSet {
        bool allow_default;
        Span<const char *> allow;
        Span<const char *> deny;
    };

    UserSet set;
    HashMap<const char *, Size> map;

    HeapArray<UnitRuleSet> rule_sets;
    BlockAllocator allow_alloc;
    BlockAllocator deny_alloc;

public:
    UserSetBuilder() = default;

    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(const StructureSet &structure_set, UserSet *out_set);

private:
    bool CheckUnitPermission(const UnitRuleSet &rule_set, const StructureEntity &ent);
};

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

static bool CheckUserName(Span<const char> name)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '.' || c == '-' || c == '_' || c == ' '; };

    if (!name.len) {
        LogError("User name cannot be empty");
        return false;
    }
    if (!std::all_of(name.begin(), name.end(), test_char)) {
        LogError("User name must only contain alphanumeric, '.', '-', '_' or ' ' characters");
        return false;
    }

    return true;
}

bool UserSetBuilder::LoadIni(StreamReader *st)
{
    K_DEFER_NC(out_guard, len = set.users.len) { set.users.RemoveFrom(len); };

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

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
            valid &= CheckUserName(prop.section);

            const char *name = DuplicateString(prop.section, &set.str_alloc).ptr;
            User *user = set.users.AppendDefault();
            UnitRuleSet rule_set = {};

            bool first_property = true;
            do {
                if (prop.key == "Template") {
                    if (first_property) {
                        Size template_idx = map.FindValue(prop.value.ptr, -1);
                        if (template_idx >= 0) {
                            MemCpy(user, &set.users[template_idx], K_SIZE(*user));
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
                    user->password_hash = DuplicateString(prop.value, &set.str_alloc).ptr;
                } else if (prop.key == "Password") {
                    if (warn_about_plain_passwords) {
                        LogError("Plain passwords are not recommended, prefer PasswordHash");
                        warn_about_plain_passwords = false;
                    }

                    if (char hash[crypto_pwhash_STRBYTES];
                            crypto_pwhash_str(hash, prop.value.ptr, prop.value.len,
                                              crypto_pwhash_OPSLIMIT_MIN,
                                              crypto_pwhash_MEMLIMIT_MIN) == 0) {
                        user->password_hash = DuplicateString(hash, &set.str_alloc).ptr;
                    } else {
                        LogError("Failed to hash password");
                        valid = false;
                    }
                } else if (prop.key == "Permissions") {
                    while (prop.value.len) {
                        bool enable;
                        Span<const char> part = SplitListValue(prop.value, &prop.value, &enable);

                        if (TestStrI(part, "All")) {
                            user->permissions = enable ? UINT_MAX : 0;
                        } else if (part.len && !OptionToFlagI(UserPermissionNames, part, &user->permissions, enable)) {
                            LogError("Unknown permission '%1'", part);
                            valid = false;
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

                        if (TestStrI(part, "All")) {
                            user->mco_dispense_modes = enable ? UINT_MAX : 0;
                        } else if (part.len && !OptionToFlagI(mco_DispenseModeOptions, part, &user->mco_dispense_modes, enable)) {
                            LogError("Unknown dispensation mode '%1'", part);
                            valid = false;
                        }
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }

                first_property = false;
            } while (ini.NextInSection(&prop));

            user->name = name;
            rule_set.allow = allow.TrimAndLeak();
            rule_set.deny = deny.TrimAndLeak();

            bool inserted;
            map.InsertOrGet(user->name, set.users.len - 1, &inserted);

            if (inserted) {
                rule_sets.Append(rule_set);
            } else {
                LogError("Duplicate user '%1'", user->name);
                valid = false;

                set.users.RemoveLast(1);
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
        Span<const char> extension = GetPathExtension(filename);

        bool (UserSetBuilder::*load_func)(StreamReader *st);
        if (extension == ".ini") {
            load_func = &UserSetBuilder::LoadIni;
        } else {
            LogError("Cannot load users from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(&st);
    }

    return success;
}

void UserSetBuilder::Finish(const StructureSet &structure_set, UserSet *out_set)
{
    K_ASSERT(set.users.len == rule_sets.len);

    for (Size i = 0; i < set.users.len; i++) {
        User &user = set.users[i];
        const UnitRuleSet &rule_set = rule_sets[i];

        for (const Structure &structure: structure_set.structures) {
            for (const StructureEntity &ent: structure.entities) {
                if (CheckUnitPermission(rule_set, ent))
                    user.mco_allowed_units.Set(ent.unit);
            }
        }

        set.map.Set(&user);
    }

    std::swap(*out_set, set);
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

bool InitUsers(const char *profile_directory)
{
    BlockAllocator temp_alloc;

    LogInfo("Load users");

    // Load INI file
    {
        const char *filename = Fmt(&temp_alloc, "%1%/users.ini", profile_directory).ptr;

        UserSetBuilder builder;
        if (!builder.LoadFiles(filename))
            return false;
        builder.Finish(thop_structure_set, &thop_user_set);
    }

    // Everyone can use the default dispense mode
    for (User &user: thop_user_set.users) {
        user.mco_dispense_modes |= 1 << (int)thop_config.mco_dispense_mode;
    }

    sessions.SetCookiePath(thop_config.base_url);

    return true;
}

const User *CheckSessionUser(http_IO *io)
{
    RetainPtr<const User> udata = sessions.Find(io);
    return udata ? udata.GetRaw() : nullptr;
}

void PruneSessions()
{
    sessions.Prune();
}

void HandleLogin(http_IO *io, const User *)
{
    const char *username = nullptr;
    const char *password = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "username") {
                    json->ParseString(&username);
                } else if (key == "password") {
                    json->ParseString(&password);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!username) {
                    LogError("Missing 'username' parameter");
                    valid = false;
                }
                if (!password) {
                    LogError("Missing 'password' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    int64_t now = GetMonotonicTime();

    // Find and validate user
    const User *user = thop_user_set.FindUser(username);
    if (!user || !user->password_hash ||
            crypto_pwhash_str_verify(user->password_hash, password, strlen(password)) != 0) {
        int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
        WaitDelay(safety_delay);

        LogError("Incorrect username or password");
        io->SendError(403);
        return;
    }

    // Create session
    RetainPtr<const User> udata((User *)user, [](User *) {});
    sessions.Open(io, udata);

    io->SendText(200, "{}", "application/json");
}

void HandleLogout(http_IO *io, const User *)
{
    sessions.Close(io);
    io->SendText(200, "{}", "application/json");
}

}
