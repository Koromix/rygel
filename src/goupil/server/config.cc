// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"

namespace RG {

bool ConfigBuilder::LoadIni(StreamReader &st)
{
    RG_DEFER_NC(out_guard, database_filename =  config.database_filename,
                           ip_stack = config.ip_stack,
                           port = config.port,
                           threads = config.threads,
                           base_url = config.base_url) {
        config.database_filename = database_filename;
        config.ip_stack = ip_stack;
        config.port = port;
        config.threads = threads;
        config.base_url = base_url;
    };

    Span<const char> root_directory;
    SplitStrReverseAny(st.filename, RG_PATH_SEPARATORS, &root_directory);

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    RG_DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Resources") {
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
                            config.ip_stack = IPStack::Dual;
                        } else if (prop.value == "IPv4") {
                            config.ip_stack = IPStack::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.ip_stack = IPStack::IPv6;
                        } else {
                            LogError("Unknown IP version '%1'", prop.value);
                        }
                    } else if (prop.key == "Port") {
                        valid &= ParseDec(prop.value, &config.port);
                    } else if (prop.key == "Threads") {
                        if (ParseDec(prop.value, &config.threads, (int)ParseFlag::End)) {
                            // Number of threads
                        } else if (prop.value == "PerConnection") {
                            config.threads = 0;
                        } else {
                            LogError("Invalid value '%1' for Threads attribute", prop.value);
                        }
                    } else if (prop.key == "BaseUrl") {
                        config.base_url = DuplicateString(prop.value, &config.str_alloc).ptr;
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
    if (ini.error || !valid)
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
        if (st.error) {
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
