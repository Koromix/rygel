// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "drdc.hh"
#include "config.hh"

namespace RG {

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> root_directory;
    SplitStrReverseAny(st->GetFileName(), RG_PATH_SEPARATORS, &root_directory);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Resources") {
                do {
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
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Institution") {
                do {
                    if (prop.key == "Sector") {
                        if (!OptionToEnum(drd_SectorNames, prop.value, &config.sector)) {
                            LogError("Unkown sector '%1'", prop.value);
                            valid = false;
                        }
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
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

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

}
