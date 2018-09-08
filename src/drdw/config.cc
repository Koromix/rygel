// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

bool UserSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.users.len) { set.users.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            User user = {};
            Size copy_from_idx = -1;
            bool changed_allow_default = false;

            // TODO: Check validity, or maybe the INI parser checks are enough?
            user.name = MakeString(&set.str_alloc, prop.section).ptr;

            do {
                if (prop.key == "PasswordHash") {
                    user.password_hash = MakeString(&set.str_alloc, prop.value).ptr;
                } else if (prop.key == "Copy") {
                    copy_from_idx = map.FindValue(prop.value.ptr, -1);
                    if (copy_from_idx < 0) {
                        LogError("Cannot copy from non-existent user '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "Default") {
                    if (prop.value == "Allow") {
                        user.allow_default = true;
                    } else if (prop.value == "Deny") {
                        user.allow_default = false;
                    } else {
                        LogError("Incorrect value '%1' for Default attribute", prop.value);
                        valid = false;
                    }
                    changed_allow_default = true;
                } else if (prop.key == "Allow") {
                    user.allow.Append(MakeString(&set.str_alloc, prop.value).ptr);
                } else if (prop.key == "Deny") {
                    user.deny.Append(MakeString(&set.str_alloc, prop.value).ptr);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (copy_from_idx >= 0) {
                const User &base_user = set.users[copy_from_idx];
                if (!changed_allow_default) {
                    user.allow_default = base_user.allow_default;
                }
                if (!user.allow.len) {
                    user.allow = base_user.allow;
                }
                if (!user.deny.len) {
                    user.deny = base_user.deny;
                }
            }

            if (map.Append(user.name, set.users.len).second) {
                set.users.Append(user);
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

void UserSetBuilder::Finish(UserSet *out_set)
{
    std::sort(set.users.begin(), set.users.end(),
              [](const User &user1, const User &user2) {
        return CmpStr(user1.name, user2.name) < 0;
    });

    for (const User &user: set.users) {
        set.map.Append(&user);
    }

    SwapMemory(out_set, &set, SIZE(set));
}

bool StructureSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.structures.len) { set.structures.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            Structure structure = {};

            // TODO: Check validity, or maybe the INI parser checks are enough?
            structure.name = MakeString(&set.str_alloc, prop.section).ptr;

            do {
                Unit unit = {};

                unit.unit = UnitCode::FromString(prop.key);
                valid &= unit.unit.IsValid();

                unit.path = MakeString(&set.str_alloc, prop.value).ptr;
                if (unit.path[0] != ':' || unit.path[1] != ':' || !unit.path[2]) {
                    LogError("Unit path does not start with '::'");
                    valid = false;
                }

                structure.units.Append(unit);
            } while (ini.NextInSection(&prop));

            if (map.Append(structure.name).second) {
                set.structures.Append(structure);
            } else {
                LogError("Duplicate structure '%1'", structure.name);
                valid = false;
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

bool StructureSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (StructureSetBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &StructureSetBuilder::LoadIni;
        } else {
            LogError("Cannot load structures from file '%1' with unknown extension '%2'",
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

void StructureSetBuilder::Finish(StructureSet *out_set)
{
    SwapMemory(out_set, &set, SIZE(set));
}
