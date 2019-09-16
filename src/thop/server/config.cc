// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "config.hh"

namespace RG {

bool ConfigBuilder::LoadIni(StreamReader &st)
{
    RG_DEFER_NC(out_guard, table_directories_len = config.table_directories.len,
                           profile_directory = config.profile_directory,
                           sector = config.sector,
                           mco_authorization_filename = config.mco_authorization_filename,
                           mco_dispense_mode = config.mco_dispense_mode,
                           mco_stay_directories_len = config.mco_stay_directories.len,
                           mco_stay_filenames_len = config.mco_stay_filenames.len,
                           http = config.http,
                           max_age = config.max_age) {
        config.table_directories.RemoveFrom(table_directories_len);
        config.profile_directory = profile_directory;
        config.sector = sector;
        config.mco_authorization_filename = mco_authorization_filename;
        config.mco_dispense_mode = mco_dispense_mode;
        config.mco_stay_directories.RemoveFrom(mco_stay_directories_len);
        config.mco_stay_filenames.RemoveFrom(mco_stay_filenames_len);
        config.http = http;
        config.max_age = max_age;
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
                        const char *const *ptr = FindIf(drd_SectorNames,
                                                        [&](const char *name) { return TestStr(name, prop.value.ptr); });
                        if (ptr) {
                            config.sector = (drd_Sector)(ptr - drd_SectorNames);
                        } else {
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
                        const OptionDesc *desc = FindIf(mco_DispenseModeOptions,
                                                        [&](const OptionDesc &desc) { return TestStr(desc.name, prop.value.ptr); });
                        if (desc) {
                            config.mco_dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
                        } else {
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
                    if (prop.key == "IPStack") {
                        if (prop.value == "Dual") {
                            config.http.ip_stack = IPStack::Dual;
                        } else if (prop.value == "IPv4") {
                            config.http.ip_stack = IPStack::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.http.ip_stack = IPStack::IPv6;
                        } else {
                            LogError("Unknown IP stack '%1'", prop.value);
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
