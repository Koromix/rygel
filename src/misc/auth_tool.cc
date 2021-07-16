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

#include "../core/libcc/libcc.hh"
#include "../core/libsecurity/libsecurity.hh"
#include "../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static int RunHashPassword(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *password = nullptr;
    bool mask = true;
    bool confirm = true;
    bool check = true;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 hash_password [options]
       %1 hash_password -p <password>%!0

Options:
    %!..+-p, --password <password>%!0    Use password given as option

        %!..+--no_mask%!0                Show password as typed
        %!..+--no_confirm%!0             Ask only once for password
        %!..+--no_check%!0               Don't check password strength)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-p", "--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--no_mask")) {
                mask = false;
            } else if (opt.Test("--no_confirm")) {
                confirm = false;
            } else if (opt.Test("--no_check")) {
                check = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    if (!password) {
retry:
        password = Prompt("Password: ", mask ? "*" : nullptr, &temp_alloc);
        if (!password)
            return 1;
        if (!password[0]) {
            LogError("Password must not be empty");
            return 1;
        }

        if (check && !sec_CheckPassword(password))
            goto retry;

        if (confirm) {
reconfirm:
            const char *confirm = Prompt("Confirm: ", mask ? "*" : nullptr, &temp_alloc);
            if (!confirm)
                return 1;

            if (!TestStr(password, confirm)) {
                LogError("Password mismatch");
                goto reconfirm;
            }
        } else if (check && !sec_CheckPassword(password)) {
            goto retry;
        }
    } else if (password[0]) {
        if (check && !sec_CheckPassword(password))
            return 1;
    } else {
        LogError("Password must not be empty");
        return 1;
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password, strlen(password),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
        return 1;
    }

    PrintLn("PasswordHash = %1", hash);
    return 0;
}

static int RunGenerateTOTP(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *label = nullptr;
    const char *issuer = nullptr;
    const char *username = nullptr;
    const char *secret = nullptr;
    int digits = 8;
    const char *png_filename = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 generate_totp [options]%!0

Options:
    %!..+-l, --label <label>%!0          Set TOTP label
    %!..+-u, --username <username>%!0    Set TOTP username
    %!..+-i, --issuer <issuer>%!0        Set TOTP issuer

    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding
    %!..+-d, --digits <digits>%!0        Use specified number of digits

    %!..+-O, --output_file <file>%!0     Write QR code PNG image to disk)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-l", "--label", OptionType::Value)) {
                label = opt.current_value;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                username = opt.current_value;
            } else if (opt.Test("-i", "--issuer", OptionType::Value)) {
                issuer = opt.current_value;
            } else if (opt.Test("-s", "--secret", OptionType::Value)) {
                secret = opt.current_value;
            } else if (opt.Test("-d", "--digits", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &digits))
                    return 1;
            } else if (opt.Test("-O", "--output_file", OptionType::Value)) {
                png_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    if (!label) {
        label = Prompt("Label: ", &temp_alloc);
        if (!label)
            return 1;
        if (!label[0]) {
            LogError("Label cannot be empty");
            return 1;
        }
    }
    if (!username) {
        username = Prompt("Username: ", &temp_alloc);
        if (!username)
            return 1;
        if (!username[0]) {
            LogError("Username cannot be empty");
            return 1;
        }
    }
    if (!issuer) {
        issuer = Prompt("Issuer: ", &temp_alloc);
        if (!issuer)
            return 1;
    }

    if (secret) {
        if (!secret[0]) {
            LogError("Empty secret is not allowed");
            return 1;
        }
    } else {
        char *ptr = (char *)Allocator::Allocate(&temp_alloc, 25, 0);
        sec_GenerateSecret(MakeSpan(ptr, 25));

        secret = ptr;
    }

    LogInfo("Secret: %!..+%1%!0", secret);
    LogInfo();

    // Generate URL
    const char *url = sec_GenerateHotpUrl(label, username, issuer, secret, digits, &temp_alloc);
    LogInfo("URL: %!..+%1%!0", url);

    if (png_filename) {
        HeapArray<uint8_t> png;
        if (!sec_GenerateHotpPng(url, &png))
            return 1;

        if (!WriteFile(png, png_filename))
            return 1;
        LogInfo("QR code written to: %!..+%1%!0", png_filename);
    }

    return 0;
}

static int RunComputeTOTP(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *secret = nullptr;
    int64_t time = GetUnixTime() / 1000;
    int digits = 8;
    int window = 0;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 compute_totp [options] <secret>%!0

Options:
    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding

    %!..+-t, --time <time>%!0            Use specified Unix time instead of current time
    %!..+-d, --digits <digits>%!0        Generate specified number of digits
                                 %!D..(default: %2)%!0
    %!..+-w, --window <window>%!0        Generate multiple codes around current time
                                 %!D..(default: %3)%!0)", FelixTarget, digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-s", "--secret", OptionType::Value)) {
                secret = opt.current_value;
            } else if (opt.Test("-t", "--time", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &time))
                    return 1;
                if (time < 0) {
                    LogError("Option --time value must be positive");
                    return 1;
                }
            } else if (opt.Test("-d", "--digits", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &digits))
                    return 1;
            } else if (opt.Test("-w", "--window", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &window))
                    return 1;
                if (window < 0) {
                    LogError("Option --window value must be positive");
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    if (!secret) {
        secret = Prompt("Secret: ", &temp_alloc);
        if (!secret)
            return 1;
        if (!secret[0]) {
            LogError("Secret must not be empty");
            return 1;
        }
    }

    for (int i = -window; i <= window; i++) {
        int code = sec_ComputeHotp(secret, time / 30 + i, digits);
        if (code < 0)
            return 1;
        PrintLn("%1", FmtArg(code).Pad0(-digits));
    }

    return 0;
}

static int RunCheckTOTP(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *secret = nullptr;
    int64_t time = GetUnixTime() / 1000;
    int digits = 8;
    int window = 0;
    const char *code = nullptr;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 check_totp [options] <secret>%!0

Options:
    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding

    %!..+-t, --time <time>%!0            Use specified Unix time instead of current time
    %!..+-d, --digits <digits>%!0        Generate specified number of digits
                                 %!D..(default: %2)%!0
    %!..+-w, --window <window>%!0        Generate multiple codes around current time
                                 %!D..(default: %3)%!0)", FelixTarget, digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-s", "--secret", OptionType::Value)) {
                secret = opt.current_value;
            } else if (opt.Test("-t", "--time", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &time))
                    return 1;
                if (time < 0) {
                    LogError("Option --time value must be positive");
                    return 1;
                }
            } else if (opt.Test("-d", "--digits", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &digits))
                    return 1;
            } else if (opt.Test("-w", "--window", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &window))
                    return 1;
                if (window < 0) {
                    LogError("Option --window value must be positive");
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    if (!secret) {
        secret = Prompt("Secret: ", &temp_alloc);
        if (!secret)
            return 1;
        if (!secret[0]) {
            LogError("Secret must not be empty");
            return 1;
        }
    }

    if (!code) {
        code = Prompt("Code: ", &temp_alloc);
        if (!code)
            return 1;
        if (strlen(code) != digits) {
            LogError("Code length does not match specified number of digits");
            return 1;
        }
    }

    if (sec_CheckHotp(secret, time / 30, digits, window, code)) {
        LogInfo("Match!");
        return 0;
    } else {
        LogError("Mismatch!");
        return 1;
    }
}

int Main(int argc, char **argv)
{
    // Options
    HeapArray<const char *> src_filenames;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+hash_password%!0                Hash a password (using libsodium)

    %!..+generate_totp%!0                Generate a TOTP QR code
    %!..+compute_totp%!0                 Generate TOTP code based on current time
    %!..+check_totp%!0                   Check TOTP code based on current time)", FelixTarget);
    };

    if (argc < 2) {
        LogError("No command provided");
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(stdout);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
        return 0;
    }

    // Execute relevant command
    if (TestStr(cmd, "hash_password")) {
        return RunHashPassword(arguments);
    } else if (TestStr(cmd, "generate_totp")) {
        return RunGenerateTOTP(arguments);
    } else if (TestStr(cmd, "compute_totp")) {
        return RunComputeTOTP(arguments);
    } else if (TestStr(cmd, "check_totp")) {
        return RunCheckTOTP(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
