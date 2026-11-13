// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

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

        valid &= config.smtp.Complete();
        valid &= config.http.Complete();
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
            } else if (prop.section == "Alerts") {
                if (prop.key == "StaleDelay") {
                    valid &= ParseDuration(prop.value, &config.stale_delay);
                } else if (prop.key == "MailDelay") {
                    valid &= ParseDuration(prop.value, &config.mail_delay);
                } else if (prop.key == "RepeatDelay") {
                    valid &= ParseDuration(prop.value, &config.repeat_delay);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
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
    if (!config.database_filename) {
        config.database_filename = NormalizePath("RekkordWeb.db", data_directory, &config.str_alloc).ptr;
    }
    if (!config.tmp_directory) {
        config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
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

}
