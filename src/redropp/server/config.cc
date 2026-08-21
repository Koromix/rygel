// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

static const int64_t DefaultDurations[] = {
    1 * 86400000ll, // 1 day
    7 * 86400000ll, // 7 day
    30 * 86400000ll, // 30 days
    90 * 86400000ll // 90 days
};
static const int64_t DefaultDuration = 7 * 86400000ll;

bool Config::Complete()
{
    if (!title) {
        const char *str = GetEnv("TITLE");
        title = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }
    if (!url) {
        const char *str = GetEnv("URL");
        url = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    if (!explicit_quota) {
        const char *str = GetEnv("DROP_QUOTA");

        if (str) {
            if (!ParseSize(str, &quota))
                return false;
            explicit_quota = true;
        }
    }

    if (!s3.Complete())
        return false;

    if (!smtp.Complete())
        return false;

    return true;
}

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

    valid &= s3.Validate();
    valid &= http.Validate();
    valid &= smtp.Validate();

    if (!internal_auth && !oidc_configs.len) {
        LogError("Cannot disable internal auth if no SSO provider is configured");
        valid = false;
    }

    for (const OidcConfig &oidc: oidc_configs) {
        valid &= oidc.provider.Validate();
    }

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
                } else if (prop.key == "URL") {
                    Span<const char> url = TrimStrRight(prop.value, '/');
                    config.url = DuplicateString(url, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
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
                    } else if (prop.key == "TempDirectory") {
                        config.tmp_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }

                    first = false;
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Drop") {
                if (prop.key == "Quota") {
                    if (ParseSize(prop.value, &config.quota)) {
                        config.explicit_quota = true;
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "ExpirationDelays") {
                    config.durations.Clear();
                    config.default_duration = 0;
                    config.allow_infinite = false;

                    Span<const char> first_duration = {};

                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));

                        if (TestStrI(part, "Never")) {
                            config.allow_infinite = true;
                        } else if (part.len) {
                            int64_t duration = 0;
                            bool set_default = false;

                            if (part[0] == '*') {
                                part = part.Take(1, part.len - 1);
                                set_default = true;
                            }

                            if (ParseDuration(part, &duration)) {
                                config.durations.Append(duration);

                                if (set_default) {
                                    if (!config.default_duration) {
                                        config.default_duration = duration;
                                    } else {
                                        LogError("Cannot set default delay multiple times");
                                        valid = false;
                                    }
                                }
                            } else {
                                valid = false;
                            }

                            if (!first_duration.len) {
                                first_duration = part;
                            }
                        }
                    }

                    if (!config.durations.len) {
                        LogError("Empty/invalid value for delays");
                        valid = false;
                    } else if (!config.default_duration) {
                        LogWarning("Missing default expiration delay, set to first choice");
                        config.default_duration = config.durations[0];
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Customize") {
                if (prop.key == "IconFile") {
                    config.custom_icon = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                } else if (prop.key == "LogoFile") {
                    config.custom_logo = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                } else if (prop.key == "StyleFile") {
                    const char *filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    config.custom_styles.Append(filename);
                } else if (prop.key == "ScriptFile") {
                    const char *filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    config.custom_scripts.Append(filename);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "S3") {
                valid &= config.s3.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "HTTP") {
                valid &= config.http.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "SMTP") {
                valid &= config.smtp.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "Authentication") {
                if (prop.key == "AllowInternal") {
                    valid &= ParseBool(prop.value, &config.internal_auth);
                } else if (prop.key == "AllowRegister") {
                    valid &= ParseBool(prop.value, &config.allow_register);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "SSO") {
                OidcConfig *oidc = config.oidc_configs.AppendDefault();

                do {
                    if (prop.key == "AutoLink") {
                        valid &= ParseBool(prop.value, &oidc->auto_link);
                    } else {
                        valid &= oidc->provider.SetProperty(prop.key, prop.value, root_directory);
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
        config.database_filename = NormalizePath("redropp.db", data_directory, &config.str_alloc).ptr;
    }
    if (!config.tmp_directory) {
        config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
    }
    if (!config.durations.len) {
        config.durations.Append(DefaultDurations);
        config.default_duration = DefaultDuration;
        config.allow_infinite = false;
    }

    // Finalize OIDC providers
    {
        for (OidcConfig &oidc: config.oidc_configs) {
            valid &= oidc.provider.Discover();
        }
        if (!valid)
            return false;

        for (const OidcConfig &oidc: config.oidc_configs) {
            if (oidc.provider.issuer) {
                config.oidc_map.Set(oidc.provider.issuer, &oidc);
            }
        }
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
