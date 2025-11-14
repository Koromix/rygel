// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

bool Config::Validate() const
{
    bool valid = true;

    valid &= http.Validate();
    valid &= !smtp.url || smtp.Validate();
    valid &= (sms.provider == sms_Provider::None) || sms.Validate();

    return valid;
}

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    config.config_filename = NormalizePath(st->GetFileName(), GetWorkingDirectory(), &config.str_alloc).ptr;

    Span<const char> root_directory = GetPathDirectory(config.config_filename);
    Span<const char> data_directory = root_directory;

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Data" || prop.section == "Paths") {
                bool first = true;

                do {
                    if (prop.key == "RootDirectory") {
                        if (first) {
                            data_directory = NormalizePath(prop.value, root_directory, &config.str_alloc);
                        } else {
                            LogError("RootDirectory must be first of section");
                            valid = false;
                        }
                    } else if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "ArchiveDirectory" || prop.key == "BackupDirectory") {
                        config.archive_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "SnapshotDirectory") {
                        config.snapshot_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "UseSnapshots") {
                        valid &= ParseBool(prop.value, &config.use_snapshots);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }

                    first = false;
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Security") {
                PasswordComplexity *ptr = nullptr;

                if (prop.key == "UserPassword") {
                    ptr = &config.user_password;
                } else if (prop.key == "AdminPassword") {
                    ptr = &config.admin_password;
                } else if (prop.key == "RootPassword") {
                    ptr = &config.root_password;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }

                if (ptr && !OptionToEnumI(PasswordComplexityNames, prop.value, ptr)) {
                    LogError("Unknown password complexity setting '%1'", prop.value);
                    valid = false;
                }

                config.custom_security = true;
            } else if (prop.section == "Demo") {
                if (prop.key == "DemoMode") {
                    valid &= ParseBool(prop.value, &config.demo_mode);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Domain") {
                LogWarning("Ignoring obsolete Domain section");
                while (ini.NextInSection(&prop));
            } else if (prop.section == "Archives") {
                LogWarning("Ignoring obsolete Archives section");
                while (ini.NextInSection(&prop));
            } else if (prop.section == "Defaults") {
                LogWarning("Ignoring obsolete Defaults section");
                while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                valid &= config.http.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "SMTP") {
                if (prop.key == "URL") {
                    config.smtp.url = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "Username") {
                    config.smtp.username = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "Password") {
                    config.smtp.password = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "From") {
                    config.smtp.from = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "SMS") {
                if (prop.key == "Provider") {
                    if (!OptionToEnumI(sms_ProviderNames, prop.value, &config.sms.provider)) {
                        LogError("Unknown SMS provider '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "AuthID") {
                    config.sms.authid = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "AuthToken") {
                    config.sms.token = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "From") {
                    config.sms.from = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    // Default values
    if (!config.database_filename) {
        config.database_filename = NormalizePath("goupile.db", data_directory, &config.str_alloc).ptr;
    }
    config.database_directory = DuplicateString(GetPathDirectory(config.database_filename), &config.str_alloc).ptr;
    config.instances_directory = NormalizePath("instances", data_directory, &config.str_alloc).ptr;
    config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
    if (!config.archive_directory) {
        config.archive_directory = NormalizePath("archives", data_directory, &config.str_alloc).ptr;
    }
    if (!config.snapshot_directory) {
        config.snapshot_directory = NormalizePath("snapshots", data_directory, &config.str_alloc).ptr;
    }
    config.view_directory = NormalizePath("views", data_directory, &config.str_alloc).ptr;
    config.export_directory = NormalizePath("exports", data_directory, &config.str_alloc).ptr;

    if (!config.Validate())
        return false;

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

}
