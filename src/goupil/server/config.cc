// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"

namespace RG {

bool ConfigBuilder::LoadIni(StreamReader &st)
{
    RG_DEFER_NC(out_guard, app_key = config.app_key,
                           database_filename = config.database_filename,
                           http = config.http,
                           max_age = config.max_age,
                           sse_keep_alive = config.sse_keep_alive) {
        config.app_key = app_key;
        config.database_filename = database_filename;
        config.http = http;
        config.max_age = max_age;
        config.sse_keep_alive = sse_keep_alive;
    };

    Span<const char> root_directory;
    SplitStrReverseAny(st.GetFileName(), RG_PATH_SEPARATORS, &root_directory);

    IniParser ini(&st);
    ini.PushLogHandler();
    RG_DEFER { PopLogHandler(); };

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
                    if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, root_directory,
                                                                 &config.str_alloc).ptr;
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
                    } else if (prop.key == "Threads") {
                        valid &= ParseDec(prop.value, &config.http.threads);
                    } else if (prop.key == "AsyncThreads") {
                        valid &= ParseDec(prop.value, &config.http.async_threads);
                    } else if (prop.key == "BaseUrl") {
                        config.http.base_url = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseDec(prop.value, &config.max_age);
                    } else if (prop.key == "SSEKeepAlive") {
                        valid &= ParseDec(prop.value, &config.sse_keep_alive);
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

    out_guard.Disable();
    return true;
}

bool ConfigBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (ConfigBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &ConfigBuilder::LoadIni;
        } else {
            LogError("Cannot load config from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
    }

    return success;
}

void ConfigBuilder::Finish(Config *out_config)
{
    SwapMemory(out_config, &config, RG_SIZE(config));
}

bool LoadConfig(Span<const char *const> filenames, Config *out_config)
{
    ConfigBuilder config_builder;
    if (!config_builder.LoadFiles(filenames))
        return false;
    config_builder.Finish(out_config);

    return true;
}

}
