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

#include "src/core/libcc/libcc.hh"
#include "src/core/libpasswd/libpasswd.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

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
        password = Prompt("Password: ", nullptr, mask ? "*" : nullptr, &temp_alloc);
        if (!password)
            return 1;

        if (check && !pwd_CheckPassword(password))
            goto retry;

        if (confirm) {
            const char *password2 = Prompt("Confirm: ", nullptr, mask ? "*" : nullptr, &temp_alloc);
            if (!password2)
                return 1;

            if (!TestStr(password, password2)) {
                LogError("Password mismatch");
                goto retry;
            }
        } else if (check && !pwd_CheckPassword(password)) {
            goto retry;
        }
    } else if (password[0]) {
        if (check && !pwd_CheckPassword(password))
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
    pwd_HotpAlgorithm algorithm = pwd_HotpAlgorithm::SHA1;
    const char *secret = nullptr;
    int digits = 6;
    const char *png_filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 generate_totp [options]%!0

Options:
    %!..+-l, --label <label>%!0          Set TOTP label
    %!..+-u, --username <username>%!0    Set TOTP username
    %!..+-i, --issuer <issuer>%!0        Set TOTP issuer

    %!..+-a, --algorithm <algorithm>%!0  Change HMAC algorithm
                                 %!D..(default: %2)%!0
    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding

    %!..+-d, --digits <digits>%!0        Use specified number of digits
                                 %!D..(default: %3)%!0

    %!..+-O, --output_file <file>%!0     Write QR code PNG image to disk)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits);
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
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnum(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
                    LogError("Unknown HMAC algorithm '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-s", "--secret", OptionType::Value)) {
                secret = opt.current_value;
            } else if (opt.Test("-d", "--digits", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &digits))
                    return 1;
                if (digits < 6 || digits > 8) {
                    LogError("Option --digits value must be between 6 and 8");
                    return 1;
                }
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
        if (!pwd_CheckSecret(secret))
            return 1;
    } else {
        Span<char> mem = AllocateSpan<char>(&temp_alloc, 33);
        pwd_GenerateSecret(mem);

        secret = mem.ptr;
    }

    LogInfo("Secret: %!..+%1%!0", secret);
    LogInfo();

    // Generate URL
    const char *url = pwd_GenerateHotpUrl(label, username, issuer, algorithm, secret, digits, &temp_alloc);
    LogInfo("URL: %!..+%1%!0", url);

    if (png_filename) {
        HeapArray<uint8_t> png;
        if (!pwd_GenerateHotpPng(url, 12, &png))
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
    pwd_HotpAlgorithm algorithm = pwd_HotpAlgorithm::SHA1;
    const char *secret = nullptr;
    int64_t time = GetUnixTime() / 1000;
    int digits = 6;
    int window = 0;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 compute_totp [options] <secret>%!0

Options:
    %!..+-a, --algorithm <algorithm>%!0  Change HMAC algorithm
                                 %!D..(default: %2)%!0
    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding

    %!..+-t, --time <time>%!0            Use specified Unix time instead of current time
    %!..+-d, --digits <digits>%!0        Generate specified number of digits
                                 %!D..(default: %3)%!0
    %!..+-w, --window <window>%!0        Generate multiple codes around current time
                                 %!D..(default: %4)%!0)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnum(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
                    LogError("Unknown HMAC algorithm '%1'", opt.current_value);
                    return 1;
                }
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
                if (digits < 6 || digits > 8) {
                    LogError("Option --digits value must be between 6 and 8");
                    return 1;
                }
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
        int code = pwd_ComputeHotp(secret, algorithm, time / 30 + i, digits);
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
    pwd_HotpAlgorithm algorithm = pwd_HotpAlgorithm::SHA1;
    const char *secret = nullptr;
    int64_t time = GetUnixTime() / 1000;
    int digits = 6;
    int window = 0;
    const char *code = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 check_totp [options] <secret>%!0

Options:
    %!..+-a, --algorithm <algorithm>%!0  Change HMAC algorithm
                                 %!D..(default: %2)%!0
    %!..+-s, --secret <secret>%!0        Set secret in Base32 encoding

    %!..+-t, --time <time>%!0            Use specified Unix time instead of current time
    %!..+-d, --digits <digits>%!0        Generate specified number of digits
                                 %!D..(default: %3)%!0
    %!..+-w, --window <window>%!0        Generate multiple codes around current time
                                 %!D..(default: %4)%!0)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnum(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
                    LogError("Unknown HMAC algorithm '%1'", opt.current_value);
                    return 1;
                }
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
                if (digits < 6 || digits > 8) {
                    LogError("Option --digits value must be between 6 and 8");
                    return 1;
                }
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

    if (secret) {
        if (!pwd_CheckSecret(secret))
            return 1;
    } else {
        secret = Prompt("Secret: ", &temp_alloc);
        if (!secret)
            return 1;
        if (!secret[0]) {
            LogError("Secret must not be empty");
            return 1;
        }
    }

    if (!code) {
        PrintLn("Expecting %1 digits", digits);

        code = Prompt("Code: ", &temp_alloc);
        if (!code)
            return 1;
        if (strlen(code) != (size_t)digits) {
            LogError("Code length does not match specified number of digits");
            return 1;
        }
    }

    int64_t counter = GetUnixTime() / 30000;
    if (pwd_CheckHotp(secret, algorithm, counter - window, counter + window, digits, code)) {
        LogInfo("Match!");
        return 0;
    } else {
        LogError("Mismatch!");
        return 1;
    }
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    HeapArray<const char *> src_filenames;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+hash_password%!0                Hash a password (using libsodium)

    %!..+generate_totp%!0                Generate a TOTP QR code
    %!..+compute_totp%!0                 Generate TOTP code based on current time
    %!..+check_totp%!0                   Check TOTP code based on current time

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        PrintLn(stderr);
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
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
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
