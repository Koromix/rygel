// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
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
            if (prop.section == "Application") {
                do {
                    if (prop.key == "Key") {
                        config.app_key = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "Name") {
                        config.app_name = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Data") {
                do {
                    if (prop.key == "LiveDirectory" || prop.key == "FilesDirectory") {
                        config.live_directory = NormalizePath(prop.value, root_directory,
                                                              &config.str_alloc).ptr;
                    } else if (prop.key == "TempDirectory") {
                        config.temp_directory = NormalizePath(prop.value, root_directory,
                                                              &config.str_alloc).ptr;
                    } else if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, root_directory,
                                                                 &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Sync") {
                do {
                    if (prop.key == "UseOffline") {
                        valid &= IniParser::ParseBoolValue(prop.value, &config.use_offline);
                    } else if (prop.key == "MaxFileSize") {
                        valid &= ParseDec(prop.value, &config.max_file_size);
                    } else if (prop.key == "SyncMode") {
                        valid &= OptionToEnum(SyncModeNames, prop.value, &config.sync_mode);
                    } else if (prop.key == "DemoUser") {
                        config.demo_user = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "IPStack") {
                        if (prop.value == "Dual") {
                            config.http.ip_stack = IPStack::Dual;
                        } else if (prop.value == "IPv4") {
                            config.http.ip_stack = IPStack::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.http.ip_stack = IPStack::IPv6;
                        } else {
                            LogError("Unknown IP version '%1'", prop.value);
                        }
                    } else if (prop.key == "Port") {
                        valid &= ParseDec(prop.value, &config.http.port);
                    } else if (prop.key == "MaxConnections") {
                        valid &= ParseDec(prop.value, &config.http.max_connections);
                    } else if (prop.key == "IdleTimeout") {
                        valid &= ParseDec(prop.value, &config.http.idle_timeout);
                    } else if (prop.key == "Threads") {
                        valid &= ParseDec(prop.value, &config.http.threads);
                    } else if (prop.key == "AsyncThreads") {
                        valid &= ParseDec(prop.value, &config.http.async_threads);
                    } else if (prop.key == "BaseUrl") {
                        config.http.base_url = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseDec(prop.value, &config.max_age);
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
    config.app_name = config.app_name ? config.app_name : config.app_key;
    if (config.database_filename && !config.temp_directory) {
        config.temp_directory = Fmt(&config.str_alloc, "%1%/tmp", root_directory).ptr;
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
