// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"

bool UserSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.users.len) { set.users.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        HeapArray<const char *> allow(&set.allow_alloc);
        HeapArray<const char *> deny(&set.deny_alloc);

        IniProperty prop;
        while (ini.Next(&prop)) {
            User user = {};
            Size copy_from_idx = -1;
            bool changed_allow_default = false;
            bool changed_dispense_modes = false;

            // TODO: Check validity, or maybe the INI parser checks are enough?
            user.name = MakeString(&set.str_alloc, prop.section).ptr;

            do {
                if (prop.key == "PasswordHash") {
                    user.password_hash = MakeString(&set.str_alloc, prop.value).ptr;
                } else if (prop.key == "Template") {
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
                    allow.Append(MakeString(&set.str_alloc, prop.value).ptr);
                } else if (prop.key == "Deny") {
                    deny.Append(MakeString(&set.str_alloc, prop.value).ptr);
                } else if (prop.key == "DispenseModes") {
                    if (prop.value == "All") {
                        user.dispense_modes = UINT_MAX;
                    } else if (prop.value == "Default") {
                        user.dispense_modes = 0;
                    } else if (prop.value.len) {
                        while (prop.value.len) {
                            Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));
                            if (part.len) {
                                const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                                      std::end(mco_DispenseModeOptions),
                                                                      [&](const OptionDesc &desc) { return TestStr(desc.name, part); });
                                if (desc == std::end(mco_DispenseModeOptions)) {
                                    LogError("Unknown dispensation mode '%1'", part);
                                    valid = false;
                                }

                                user.dispense_modes |= 1 << (int)(desc - mco_DispenseModeOptions);
                            }
                        }
                    } else {
                        LogError("Incorrect value '%1' for DispenseModes attribute", prop.value);
                        valid = false;
                    }
                    changed_dispense_modes = true;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            user.allow = allow.TrimAndLeak();
            user.deny = deny.TrimAndLeak();

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
                if (!changed_dispense_modes) {
                    user.dispense_modes = base_user.dispense_modes;
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
            if (!prop.section.len) {
                if (prop.key == "DispenseMode") {
                    const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                          std::end(mco_DispenseModeOptions),
                                                          [&](const OptionDesc &desc) { return TestStr(desc.name, prop.value.ptr); });
                    if (desc == std::end(mco_DispenseModeOptions)) {
                        LogError("Unknown dispensation mode '%1'", prop.value);
                        valid = false;
                    }
                    set.dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else {
                Structure structure = {};

                // TODO: Check validity, or maybe the INI parser checks are enough?
                structure.name = MakeString(&set.str_alloc, prop.section).ptr;

                do {
                    Unit unit = {};

                    unit.unit = UnitCode::FromString(prop.key);
                    valid &= unit.unit.IsValid();

                    unit.path = MakeString(&set.str_alloc, prop.value).ptr;
                    if (unit.path[0] != '|' || !unit.path[1]) {
                        LogError("Unit path does not start with '|'");
                        valid = false;
                    }

                    structure.units.Append(unit);
                } while (ini.NextInSection(&prop));

                std::sort(structure.units.begin(), structure.units.end(),
                          [](const Unit &unit1, const Unit &unit2) {
                    return CmpStr(unit1.path, unit2.path) < 0;
                });

                if (map.Append(structure.name).second) {
                    set.structures.Append(structure);
                } else {
                    LogError("Duplicate structure '%1'", structure.name);
                    valid = false;
                }
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
