// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
#include "drdc.hh"
#include "config.hh"

namespace K {

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Resources") {
                if (prop.key == "TableDirectory") {
                    const char *directory = NormalizePath(prop.value, root_directory,
                                                          &config.str_alloc).ptr;
                    config.table_directories.Append(directory);
                } else if (prop.key == "ProfileDirectory") {
                    config.profile_directory = NormalizePath(prop.value, root_directory,
                                                             &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Institution") {
                if (prop.key == "Sector") {
                    if (!OptionToEnumI(drd_SectorNames, prop.value, &config.sector)) {
                        LogError("Unkown sector '%1'", prop.value);
                        valid = false;
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "MCO") {
                if (prop.key == "AuthorizationFile") {
                    config.mco_authorization_filename = NormalizePath(prop.value, root_directory,
                                                                      &config.str_alloc).ptr;
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
    if (!config.table_directories.len) {
        const char *directory = NormalizePath("tables", root_directory, &config.str_alloc).ptr;
        config.table_directories.Append(directory);
    }
    if (!config.profile_directory) {
        config.profile_directory = NormalizePath("profile", root_directory, &config.str_alloc).ptr;
    }

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

}
