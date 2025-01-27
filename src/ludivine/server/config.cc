// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "config.hh"

namespace RG {

bool Config::Validate() const
{
    bool valid = true;

    if (!title || !title[0]) {
        LogError("Missing main title");
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

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "General") {
                if (prop.key == "Title") {
                    config.title = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "PublicURL") {
                    Span<const char> url = TrimStrRight(prop.value, '/');
                    config.url = DuplicateString(url, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Data") {
                if (prop.key == "DatabaseFile") {
                    config.database_filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                } else if (prop.key == "VaultDirectory") {
                    config.vault_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "RequireHost") {
                        config.require_host = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        valid &= config.http.SetProperty(prop.key.ptr, prop.value.ptr, root_directory);
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "SMTP") {
                do {
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
                } while (ini.NextInSection(&prop));
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
        config.database_filename = NormalizePath("ludivine.db", root_directory, &config.str_alloc).ptr;
    }
    if (!config.vault_directory) {
        config.vault_directory = NormalizePath("vaults", root_directory, &config.str_alloc).ptr;
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
