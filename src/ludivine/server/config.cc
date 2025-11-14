// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

bool Config::Validate() const
{
    bool valid = true;

    if (!title || !title[0]) {
        LogError("Missing main title");
        valid = false;
    }
    if (!contact || !contact[0]) {
        LogError("Missing main contact");
        valid = false;
    }
    if (!url || !url[0]) {
        LogError("Missing public URL");
        valid = false;
    }

    valid &= http.Validate();
    valid &= smtp.Validate();

    return valid;
}

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    const char *config_filename = NormalizePath(st->GetFileName(), GetWorkingDirectory(), &config.str_alloc).ptr;
    Span<const char> root_directory = GetPathDirectory(config_filename);
    Span<const char> data_directory = root_directory;

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "General") {
                if (prop.key == "Title") {
                    config.title = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "Contact") {
                    config.contact = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "URL") {
                    Span<const char> url = TrimStrRight(prop.value, '/');
                    config.url = DuplicateString(url, &config.str_alloc).ptr;
                } else if (prop.key == "TestMode") {
                    valid &= ParseBool(prop.value, &config.test_mode);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Pages") {
                Config::PageInfo page = {};

                page.title = DuplicateString(prop.key, &config.str_alloc).ptr;
                page.url = DuplicateString(prop.value, &config.str_alloc).ptr;

                config.pages.Append(page);
            } else if (prop.section == "Data") {
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
                    } else if (prop.key == "VaultDirectory") {
                        config.vault_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "TempDirectory") {
                        config.tmp_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "StaticDirectory") {
                        config.static_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }

                    first = false;
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                valid &= config.http.SetProperty(prop.key.ptr, prop.value.ptr, root_directory);
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
        config.database_filename = NormalizePath("ludivine.db", data_directory, &config.str_alloc).ptr;
    }
    if (!config.vault_directory) {
        config.vault_directory = NormalizePath("vaults", data_directory, &config.str_alloc).ptr;
    }
    if (!config.tmp_directory) {
        config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
    }
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
