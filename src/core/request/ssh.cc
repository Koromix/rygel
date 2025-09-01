// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "curl.hh"
#include "ssh.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

K_INIT(libssh)
{
    K_CRITICAL(!ssh_init(), "Failed to initialize libssh");
}

K_FINALIZE(libssh)
{
    ssh_finalize();
}

bool ssh_Config::SetProperty(Span<const char> key, Span<const char> value, Span<const char>)
{
    if (key == "Location") {
        return ssh_DecodeURL(value, this);
    } else if (key == "Host") {
        host = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Port") {
        return ParseInt(value, &port);
    } else if (key == "User") {
        username = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Path") {
        path = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "KnownHosts") {
        return ParseBool(value, &known_hosts);
    } else if (key == "Fingerprint") {
        fingerprint = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Password") {
        password = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "Key") {
        key = DuplicateString(value, &str_alloc).ptr;
        return true;
    } else if (key == "KeyFile") {
        keyfile = DuplicateString(value, &str_alloc).ptr;
        return true;
    }

    LogError("Unknown SSH property '%1'", key);
    return false;
}

bool ssh_Config::Complete()
{
    if (!password && !keyfile && !key) {
        if (const char *str = GetEnv("SSH_KEY"); str) {
            key = DuplicateString(str, &str_alloc).ptr;
        } else if (const char *str = GetEnv("SSH_KEYFILE"); str) {
            keyfile = DuplicateString(str, &str_alloc).ptr;
        } else if (const char *str = GetEnv("SSH_PASSWORD"); str) {
            password = DuplicateString(str, &str_alloc).ptr;
        } else if (username && FileIsVt100(STDERR_FILENO)) {
            password = Prompt(T("SSH password:"), nullptr, "*", &str_alloc);
            if (!password)
                return false;
        }
    }

    if (!fingerprint) {
        const char *str = GetEnv("SSH_FINGERPRINT");
        fingerprint = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    return true;
}

bool ssh_Config::Validate() const
{
    bool valid = true;

    if (!host) {
        LogError("Missing SFTP host name");
        valid = false;
    }
    if (!port || port > 65535) {
        LogError("Invalid SFTP port");
        valid = false;
    }
    if (!username) {
        LogError("Missing SFTP username");
        valid = false;
    }

    if (!known_hosts && !fingerprint) {
        LogError("Cannot use SFTP without known Fingerprint and without using KnownHosts");
        valid = false;
    }
    if (!password && !key && !keyfile) {
        LogError("Missing SFTP password (SSH_PASSWORD) and/or key (SSH_KEY or SSH_KEYFILE)");
        valid = false;
    }

    return valid;
}

void ssh_Config::Clone(ssh_Config *out_config) const
{
    out_config->str_alloc.Reset();

    out_config->host = host ? DuplicateString(host, &out_config->str_alloc).ptr : nullptr;
    out_config->port = port;
    out_config->username = username ? DuplicateString(username, &out_config->str_alloc).ptr : nullptr;
    out_config->path = path ? DuplicateString(path, &out_config->str_alloc).ptr : nullptr;
    out_config->known_hosts = known_hosts;
    out_config->fingerprint = fingerprint ? DuplicateString(fingerprint, &out_config->str_alloc).ptr : nullptr;
    out_config->password = password ? DuplicateString(password, &out_config->str_alloc).ptr : nullptr;
    out_config->key = key ? DuplicateString(key, &out_config->str_alloc).ptr : nullptr;
    out_config->keyfile = keyfile ? DuplicateString(keyfile, &out_config->str_alloc).ptr : nullptr;
}

bool ssh_DecodeURL(Span<const char> url, ssh_Config *out_config)
{
    CURLU *h = curl_url();
    K_DEFER { curl_url_cleanup(h); };

    if (StartsWith(url, "ssh://") || StartsWith(url, "sftp://")) {
        char url0[32768];
        Fmt(url0, "%1", url);

        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url0, CURLU_NON_SUPPORT_SCHEME);

        if (ret != CURLUE_OK) {
            LogError("Failed to parse URL '%1': %2", url, curl_url_strerror(ret));
            return false;
        }

        out_config->host = curl_GetUrlPartStr(h, CURLUPART_HOST, &out_config->str_alloc).ptr;
        out_config->port = curl_GetUrlPartInt(h, CURLUPART_PORT);
        out_config->username = curl_GetUrlPartStr(h, CURLUPART_USER, &out_config->str_alloc).ptr;
        out_config->path = curl_GetUrlPartStr(h, CURLUPART_PATH, &out_config->str_alloc).ptr;

        // The first '/' separates the host from the path, use '//' for absolute path
        if (out_config->path && out_config->path[0] == '/') {
            out_config->path++;
        }
    } else {
        Span<const char> remain = url;

        Span<const char> username = SplitStr(remain, '@', &remain);
        Span<const char> host = SplitStr(remain, ':', &remain);
        Span<const char> path = remain;

        if (host.ptr == username.end() || path.ptr == host.end()) {
            LogError("Failed to parse SSH URL, expected <user>@<host>");
            return false;
        }

        out_config->host = DuplicateString(host, &out_config->str_alloc).ptr;
        out_config->port = 22;
        out_config->username = DuplicateString(username, &out_config->str_alloc).ptr;
        out_config->path = DuplicateString(path, &out_config->str_alloc).ptr;
    }

    return true;
}

static bool SetStringOption(ssh_session ssh, ssh_options_e type, const char *str)
{
    int ret = ssh_options_set(ssh, type, str);
    return (ret == SSH_OK);
}

template <typename T>
bool SetIntegerOption(ssh_session ssh, ssh_options_e type, T value)
{
    int ret = ssh_options_set(ssh, type, &value);
    return (ret == SSH_OK);
}

ssh_session ssh_Connect(const ssh_Config &config)
{
    if (!config.Validate())
        return nullptr;

    ssh_session ssh = ssh_new();
    if (!ssh)
        K_BAD_ALLOC();

    K_DEFER_N(err_guard) {
        if (ssh_is_connected(ssh)) {
            ssh_disconnect(ssh);
        }
        ssh_free(ssh);
    };

    // Set options
    {
        bool success = true;

        success &= SetStringOption(ssh, SSH_OPTIONS_HOST, config.host);
        success &= SetIntegerOption(ssh, SSH_OPTIONS_PORT, config.port > 0 ? config.port : 22);
        success &= SetStringOption(ssh, SSH_OPTIONS_USER, config.username);
        success &= SetIntegerOption(ssh, SSH_OPTIONS_TIMEOUT_USEC, 60000000L);

        if (!success)
            return nullptr;
    }

    // Connect SSH
    if (ssh_connect(ssh) != SSH_OK) {
        LogError("Failed to connect to '%1': %2", config.host, ssh_get_error(ssh));
        return nullptr;
    }

    // Authenticate server
    {
        ssh_key pk;
        Span<uint8_t> hash;

        if (ssh_get_server_publickey(ssh, &pk) < 0) {
            LogError("Failed to retrieve SSH public key of '%1': %2", config.host, ssh_get_error(ssh));
            return nullptr;
        }
        K_DEFER { ssh_key_free(pk); };

        size_t hash_len;
        if (ssh_get_publickey_hash(pk, SSH_PUBLICKEY_HASH_SHA256, &hash.ptr, &hash_len) < 0) {
            LogError("Failed to hash SSH public key of '%1': %2", config.host, ssh_get_error(ssh));
            return nullptr;
        }
        hash.len = (Size)hash_len;
        K_DEFER { ssh_clean_pubkey_hash(&hash.ptr); };

        ssh_known_hosts_e state = ssh_session_is_known_server(ssh);

        switch (state) {
            case SSH_KNOWN_HOSTS_OK: { /* LogInfo("OK"); */ } break;

            case SSH_KNOWN_HOSTS_CHANGED:
            case SSH_KNOWN_HOSTS_OTHER: {
                LogError("Host key has changed, possible attack");
                return nullptr;
            } break;

            case SSH_KNOWN_HOSTS_NOT_FOUND:
            case SSH_KNOWN_HOSTS_UNKNOWN: {
                char base64[256] = {};
                K_ASSERT(sodium_base64_ENCODED_LEN(hash.len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING) < K_SIZE(base64));

                CopyString("SHA256:", base64);
                sodium_bin2base64(base64 + 7, K_SIZE(base64) - 7, hash.ptr, hash.len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

                if (config.fingerprint && TestStr(base64, config.fingerprint))
                    break;

                LogInfo("The server is unknown, public key hash: %!..+%1%!0", base64);

                bool trust = false;
                {
                    Size idx = PromptEnum("Do you trust the host key? ", {{'y', "Yes"}, {'n', "No"}}, 1);
                    if (idx < 0)
                        return nullptr;
                    trust = !idx;
                }
                if (!trust) {
                    LogError("Cannot trust server, refusing to continue");
                    return nullptr;
                }

                if (ssh_session_update_known_hosts(ssh) < 0) {
                    LogError("Failed to update known_hosts file: %1", strerror(errno));
                    return nullptr;
                }
            } break;

            case SSH_KNOWN_HOSTS_ERROR: {
                LogInfo("Host error: %1", ssh_get_error(ssh));
                return nullptr;
            } break;
        }
    }

    // Authenticate user
    if (config.key) {
        ssh_key private_key = nullptr;
        if (ssh_pki_import_privkey_base64(config.key, nullptr, nullptr, nullptr, &private_key) < 0) {
            LogError("Failed to import private key string");
            return nullptr;
        }
        K_DEFER { ssh_key_free(private_key); };

        if (ssh_userauth_publickey(ssh, nullptr, private_key) != SSH_AUTH_SUCCESS) {
            LogError("Failed to authenticate to '%1@%2': %3", config.username, config.host, ssh_get_error(ssh));
            return nullptr;
        }
    }
    if (config.keyfile) {
        ssh_key private_key = nullptr;
        if (ssh_pki_import_privkey_file(config.keyfile, nullptr, nullptr, nullptr, &private_key) < 0) {
            LogError("Failed to load private key from '%1'", config.keyfile);
            return nullptr;
        }
        K_DEFER { ssh_key_free(private_key); };

        if (ssh_userauth_publickey(ssh, nullptr, private_key) != SSH_AUTH_SUCCESS) {
            LogError("Failed to authenticate to '%1@%2': %3", config.username, config.host, ssh_get_error(ssh));
            return nullptr;
        }
    } else {
        K_ASSERT(config.password);

        if (ssh_userauth_password(ssh, nullptr, config.password) != SSH_AUTH_SUCCESS) {
            LogError("Failed to authenticate to '%1@%2': %3", config.username, config.host, ssh_get_error(ssh));
            return nullptr;
        }
    }

    err_guard.Disable();
    return ssh;
}

const char *TranslateSftpError(int error)
{
    switch (error) {
        case SSH_FX_OK: return "Success";
        case SSH_FX_EOF: return "End-of-file encountered";
        case SSH_FX_NO_SUCH_FILE: return "File doesn't exist";
        case SSH_FX_PERMISSION_DENIED: return "Permission denied";
        case SSH_FX_FAILURE: return "Generic failure";
        case SSH_FX_BAD_MESSAGE: return "Garbage received from server";
        case SSH_FX_NO_CONNECTION: return "No connection has been set up";
        case SSH_FX_CONNECTION_LOST: return "There was a connection, but we lost it";
        case SSH_FX_OP_UNSUPPORTED: return "Operation not supported by the server";
        case SSH_FX_INVALID_HANDLE: return "Invalid file handle";
        case SSH_FX_NO_SUCH_PATH: return "No such file or directory path exists";
        case SSH_FX_FILE_ALREADY_EXISTS: return "An attempt to create an already existing file or directory has been made";
        case SSH_FX_WRITE_PROTECT: return "We are trying to write on a write-protected filesystem";
        case SSH_FX_NO_MEDIA: return "No media in remote drive";
    }

    return "Unknown error";
}

const char *sftp_GetErrorString(sftp_session sftp)
{
    int error = sftp_get_error(sftp);

    if (error != SSH_FX_OK) {
        return TranslateSftpError(error);
    } else {
        return ssh_get_error(sftp->session);
    }
}

}
