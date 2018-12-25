// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
#include "config.hh"

bool ConfigBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, table_directories_len = config.table_directories.len,
                        profile_directory = config.profile_directory,
                        authorization_filename = config.authorization_filename,
                        mco_stay_directories_len = config.mco_stay_directories.len,
                        mco_stay_filenames_len = config.mco_stay_filenames.len,
                        port = config.port,
                        dispense_mode = config.dispense_mode) {
        config.table_directories.RemoveFrom(table_directories_len);
        config.profile_directory = profile_directory;
        config.authorization_filename = authorization_filename;
        config.mco_stay_directories.RemoveFrom(mco_stay_directories_len);
        config.mco_stay_filenames.RemoveFrom(mco_stay_filenames_len);
        config.port = port;
        config.dispense_mode = dispense_mode;
    };

    Span<const char> root_dir;
    SplitStrReverseAny(st.filename, PATH_SEPARATORS, &root_dir);

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Resources") {
                do {
                    if (prop.key == "TableDirectory") {
                        config.table_directories.Append(CanonicalizePath(root_dir, prop.value.ptr, &config.str_alloc));
                    } else if (prop.key == "ProfileDirectory") {
                        config.profile_directory = CanonicalizePath(root_dir, prop.value.ptr, &config.str_alloc);
                    } else if (prop.key == "AuthorizationFile") {
                        config.authorization_filename = CanonicalizePath(root_dir, prop.value.ptr, &config.str_alloc);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "MCO") {
                do {
                    if (prop.key == "DispenseMode") {
                        const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                              std::end(mco_DispenseModeOptions),
                                                              [&](const OptionDesc &desc) { return TestStr(desc.name, prop.value.ptr); });
                        if (desc == std::end(mco_DispenseModeOptions)) {
                            LogError("Unknown dispensation mode '%1'", prop.value);
                            valid = false;
                        }
                        config.dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
                    } else if (prop.key == "StayDirectory") {
                        config.mco_stay_directories.Append(CanonicalizePath(root_dir, prop.value.ptr, &config.str_alloc));
                    } else if (prop.key == "StayFile") {
                        config.mco_stay_filenames.Append(CanonicalizePath(root_dir, prop.value.ptr, &config.str_alloc));
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "IPVersion") {
                        if (prop.value == "Dual") {
                            config.ip_version = Config::IPVersion::Dual;
                        } else if (prop.value == "IPv4") {
                            config.ip_version = Config::IPVersion::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.ip_version = Config::IPVersion::IPv6;
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

    out_guard.disable();
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
    SwapMemory(out_config, &config, SIZE(config));
}

bool LoadConfig(Span<const char *const> filenames, Config *out_config)
{
    ConfigBuilder config_builder;
    if (!config_builder.LoadFiles(filenames))
        return false;
    config_builder.Finish(out_config);

    return true;
}
