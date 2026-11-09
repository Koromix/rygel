// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

static const int64_t DefaultDurations[] = {
    1 * 86400000ll, // 1 day
    3 * 86400000ll, // 3 days
    7 * 86400000ll, // 7 days
    30 * 86400000ll, // 30 days
    90 * 86400000ll // 90 days
};
static const int64_t DefaultDuration = 7 * 86400000ll;

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

static bool ParseExpirationDelays(Span<const char> str, HeapArray<int64_t> *out_durations, int64_t *out_default, bool *out_infinite)
{
    HeapArray<int64_t> durations;
    int64_t default_duration = 0;
    bool allow_infinite = false;

    Span<const char> first_duration = {};

    while (str.len) {
        Span<const char> part = TrimStr(SplitStrAny(str, " ,", &str));

        if (TestStrI(part, "Never")) {
            allow_infinite = true;
        } else if (part.len) {
            int64_t duration = 0;
            bool set_default = false;

            if (part[0] == '*') {
                part = part.Take(1, part.len - 1);
                set_default = true;
            }

            if (ParseDuration(part, &duration)) {
                durations.Append(duration);

                if (set_default) {
                    if (!default_duration) {
                        default_duration = duration;
                    } else {
                        LogError("Cannot set default delay multiple times");
                        return false;
                    }
                }
            } else {
                return false;
            }

            if (!first_duration.len) {
                first_duration = part;
            }
        }
    }

    if (!durations.len) {
        LogError("Empty/invalid value for delays");
        return false;
    } else if (!default_duration) {
        LogWarning("Missing default expiration delay, set to first choice");
        default_duration = durations[0];
    }

    std::swap(*out_durations, durations);
    *out_default = default_duration;
    *out_infinite = allow_infinite;

    return true;
}

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> data_directory = {};
    bool valid = true;

    // Read environment config
    {
        if (const char *str = GetEnv("TITLE")) {
            config.title = DuplicateString(str, &config.str_alloc).ptr;
        }
        if (const char *str = GetEnv("URL")) {
            config.url = DuplicateString(str, &config.str_alloc).ptr;
        }

        if (const char *str = GetEnv("DROP_QUOTA")) {
            valid &= ParseSize(str, &config.quota);
        }
        if (const char *str = GetEnv("DROP_DELAYS")) {
            valid &= ParseExpirationDelays(str, &config.durations, &config.default_duration, &config.allow_infinite);
        }

        if (const char *str = GetEnv("AUTH_INTERNAL")) {
            valid &= ParseBool(str, &config.internal_auth);
        }
        if (const char *str = GetEnv("AUTH_REGISTER")) {
            valid &= ParseBool(str, &config.allow_register);
        }
        if (GetEnv("SSO_TITLE")) {
            OidcConfig *oidc = config.oidc_configs.AppendDefault();
            valid &= oidc->provider.Complete();
        }

        valid &= config.s3.Complete(false);
        valid &= config.smtp.Complete();

        if (const char *str = GetEnv("HTTP_PORT")) {
            valid &= config.http.SetPortOrPath(str);
        }
    }

    if (st) {
        const char *config_filename = NormalizePath(st->GetFileName(), GetWorkingDirectory(), &config.str_alloc).ptr;
        Span<const char> root_directory = GetPathDirectory(config_filename);

        data_directory = root_directory;

        IniParser ini(st);
        ini.PushLogFilter();
        K_DEFER { PopLogFilter(); };

        for (IniProperty prop; ini.Next(&prop); ) {
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
                    valid &= ParseExpirationDelays(prop.value, &config.durations, &config.default_duration, &config.allow_infinite);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Customize") {
                if (prop.key == "Contact") {
                    config.contact = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "IconFile") {
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
                if (prop.key == "SecureCookies") {
                    valid &= ParseBool(prop.value, &config.secure_cookies);
                } else {
                    valid &= config.http.SetProperty(prop.key, prop.value, root_directory);
                }
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
        if (!ini.IsValid())
            return false;
    }

    if (!valid)
        return false;

    // Default values
    {
        if (!config.database_filename) {
            const char *compat_filename = NormalizePath("redropp.db", data_directory, &config.str_alloc).ptr;

            if (TestFile(compat_filename)) {
                config.database_filename = compat_filename;
            } else {
                config.database_filename = NormalizePath("data/redropp.db", data_directory, &config.str_alloc).ptr;
            }
        }
        if (!config.tmp_directory) {
            config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
        }

        if (!config.durations.len) {
            config.durations.Append(DefaultDurations);
            config.default_duration = DefaultDuration;
            config.allow_infinite = false;
        }
    }

    // Finalize OIDC providers
    {
        bool valid = true;

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

}
