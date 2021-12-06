// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"

namespace RG {

bool Config::Validate() const
{
    bool valid = true;

    valid &= http.Validate();
    if (base_url[0] != '/' || base_url[strlen(base_url) - 1] != '/') {
        LogError("Base URL '%1' does not start and end with '/'", base_url);
        valid = false;
    }
    if (max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }

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
                do {
                    if (prop.key == "AuthorizationFile") {
                        config.mco_authorization_filename = NormalizePath(prop.value, root_directory,
                                                                          &config.str_alloc).ptr;
                    } else if (prop.key == "DispenseMode") {
                        if (!OptionToEnum(mco_DispenseModeOptions, prop.value, &config.mco_dispense_mode)) {
                            LogError("Unknown dispensation mode '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "StayDirectory") {
                        const char *directory = NormalizePath(prop.value, root_directory,
                                                              &config.str_alloc).ptr;
                        config.mco_stay_directories.Append(directory);
                    } else if (prop.key == "StayFile") {
                        const char *filename = NormalizePath(prop.value, root_directory,
                                                             &config.str_alloc).ptr;
                        config.mco_stay_filenames.Append(filename);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "SocketType" || prop.key == "IPStack") {
                        if (!OptionToEnum(SocketTypeNames, prop.value, &config.http.sock_type)) {
                            LogError("Unknown socket type '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "UnixPath") {
                        config.http.unix_path = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "Port") {
                        valid &= ParseInt(prop.value, &config.http.port);
                    } else if (prop.key == "MaxConnections") {
                        valid &= ParseInt(prop.value, &config.http.max_connections);
                    } else if (prop.key == "IdleTimeout") {
                        valid &= ParseInt(prop.value, &config.http.idle_timeout);
                    } else if (prop.key == "Threads") {
                        valid &= ParseInt(prop.value, &config.http.threads);
                    } else if (prop.key == "AsyncThreads") {
                        valid &= ParseInt(prop.value, &config.http.async_threads);
                    } else if (prop.key == "BaseUrl") {
                        config.base_url = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "ClientAddress") {
                        if (!OptionToEnum(http_ClientAddressModeNames, prop.value, &config.http.client_addr_mode)) {
                            LogError("Unknown client address mode '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseInt(prop.value, &config.max_age);
                    } else if (prop.key == "RequireHost") {
                        config.require_host = DuplicateString(prop.value, &config.str_alloc).ptr;
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
    if (!config.table_directories.len) {
        const char *directory = NormalizePath("tables", root_directory, &config.str_alloc).ptr;
        config.table_directories.Append(directory);
    }
    if (!config.profile_directory) {
        config.profile_directory = NormalizePath("profile", root_directory, &config.str_alloc).ptr;
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
