// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "src/core/password/otp.hh"
#include "src/core/password/password.hh"
#include "src/core/wrap/qrcode.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static int RunGeneratePassword(Span<const char *> arguments)
{
    const int MaxPasswordLength = 256;

    // Options
    int length = 32;
    const char *pattern = "luds";
    bool check = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 generate_password [option...]%!0

Options:

    %!..+-l, --length length%!0            Set desired password length
                                   %!D..(default: %2)%!0
    %!..+-p, --pattern chars%!0            Set allowed/required characters, see below
                                   %!D..(default: %3)%!0

        %!..+--no_check%!0                 Don't check password strength

Use a pattern to set which characters classes are present in the password:

    %!..+l%!0                              Use non-ambiguous lowercase characters
    %!..+L%!0                              Use all lowercase characters (including l)
    %!..+u%!0                              Use non-ambiguous uppercase characters
    %!..+U%!0                              Use all lowercase characters (including I and O)
    %!..+d%!0                              Use non-ambiguous digits
    %!..+D%!0                              Use all digits (including 1 and 0)
    %!..+s%!0                              Use basic special symbols
    %!..+!%!0                              Use dangerous special symbols
                                   %!D..(annoying to type or to use in terminals)%!0

Here are a few example patterns:

    %!..+LUD%!0                            Use all characters (lower and uppercase) and digits
    %!..+lus%!0                            Use non-ambiguous characters (lower and uppercase) and basic special symbols
    %!..+D!%!0                             Use all digits and dangerous special symbols)", FelixTarget, length, pattern);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-l", "--length", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &length))
                    return 1;
                if (length <= 0 || length > MaxPasswordLength) {
                    LogError("Password length must be between 0 and %1", MaxPasswordLength);
                    return 1;
                }
            } else if (opt.Test("-p", "--pattern", OptionType::Value)) {
                pattern = opt.current_value;
            } else if (opt.Test("--no_check")) {
                check = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    LocalArray<char, MaxPasswordLength + 1> password_buf;
    password_buf.len = length + 1;

    unsigned int flags = 0;
    for (Size i = 0; pattern[i]; i++) {
        char c = pattern[i];

        switch (c) {
            case 'l': { flags |= (int)pwd_GenerateFlag::LowersNoAmbi; } break;
            case 'L': { flags |= (int)pwd_GenerateFlag::Lowers; } break;
            case 'u': { flags |= (int)pwd_GenerateFlag::UppersNoAmbi; } break;
            case 'U': { flags |= (int)pwd_GenerateFlag::Uppers; } break;
            case 'd': { flags |= (int)pwd_GenerateFlag::DigitsNoAmbi; } break;
            case 'D': { flags |= (int)pwd_GenerateFlag::Digits; } break;
            case 's': { flags |= (int)pwd_GenerateFlag::Specials; } break;
            case '!': { flags |= (int)pwd_GenerateFlag::Dangerous; } break;

            default: {
                if ((uint8_t)c < 32 || (uint8_t)c >= 128) {
                    LogError("Illegal pattern byte 0x%1", FmtHex((uint8_t)c).Pad0(-2));
                } else {
                    LogError("Unsupported pattern character '%1'", c);
                }

                return 1;
            } break;
        }
    }
    flags = ApplyMask(flags, (int)pwd_GenerateFlag::Check, check);

    if (!pwd_GeneratePassword(flags, password_buf))
        return 1;

    LogInfo("Password: %!..+%1%!0", password_buf.data);
    return 0;
}

static int RunHashPassword(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *password = nullptr;
    bool mask = true;
    bool confirm = true;
    bool check = true;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 hash_password [option...]
       %1 hash_password -p password%!0

Options:

    %!..+-p, --password password%!0        Use password given as option

        %!..+--no_mask%!0                  Show password as typed
        %!..+--no_confirm%!0               Ask only once for password
        %!..+--no_check%!0                 Don't check password strength)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
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

        opt.LogUnusedArguments();
    }

    if (!password) {
retry:
        password = Prompt(T("Password:"), nullptr, mask ? "*" : nullptr, &temp_alloc);
        if (!password)
            return 1;

        if (confirm) {
            const char *password2 = Prompt(T("Confirm:"), nullptr, mask ? "*" : nullptr, &temp_alloc);
            if (!password2)
                return 1;

            if (!TestStr(password, password2)) {
                LogError("Password mismatch");
                goto retry;
            }
        }

        if (check && !pwd_CheckPassword(password))
            goto retry;
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

static int RunCheckPassword(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *password = nullptr;
    bool mask = true;
    bool confirm = true;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 check_password [option...]
       %1 check_password -p password%!0

Options:

    %!..+-p, --password password%!0        Use password given as option

        %!..+--no_mask%!0                  Show password as typed
        %!..+--no_confirm%!0               Ask only once for password)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-p", "--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--no_mask")) {
                mask = false;
            } else if (opt.Test("--no_confirm")) {
                confirm = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!password) {
retry:
        password = Prompt(T("Password:"), nullptr, mask ? "*" : nullptr, &temp_alloc);
        if (!password)
            return 1;

        if (confirm) {
            const char *password2 = Prompt(T("Confirm:"), nullptr, mask ? "*" : nullptr, &temp_alloc);
            if (!password2)
                return 1;

            if (!TestStr(password, password2)) {
                LogError("Password mismatch");
                goto retry;
            }
        }
    } else {
        LogError("Password must not be empty");
        return 1;
    }

    if (!pwd_CheckPassword(password))
        return 1;

    LogInfo("Valid password");
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
    bool skip_qrcode = false;
    const char *png_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 generate_totp [option...]%!0

Options:

    %!..+-l, --label label%!0              Set TOTP label
    %!..+-u, --username username%!0        Set TOTP username
    %!..+-i, --issuer issuer%!0            Set TOTP issuer

    %!..+-a, --algorithm algorithm%!0      Change HMAC algorithm
                                   %!D..(default: %2)%!0
    %!..+-s, --secret secret%!0            Set secret in Base32 encoding

    %!..+-d, --digits digits%!0            Use specified number of digits
                                   %!D..(default: %3)%!0

        %!..+--skip_qrcode%!0              Skip generation of QR code
    %!..+-P, --png_file filename%!0        Write QR code PNG image to disk)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-l", "--label", OptionType::Value)) {
                label = opt.current_value;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                username = opt.current_value;
            } else if (opt.Test("-i", "--issuer", OptionType::Value)) {
                issuer = opt.current_value;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnumI(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
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
            } else if (opt.Test("--skip_qrcode")) {
                skip_qrcode = true;
            } else if (opt.Test("-P", "--png_file", OptionType::Value)) {
                png_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!label) {
        label = Prompt(T("Label:"), &temp_alloc);
        if (!label)
            return 1;
        if (!label[0]) {
            LogError("Label cannot be empty");
            return 1;
        }
    }
    if (!username) {
        username = Prompt(T("Username:"), &temp_alloc);
        if (!username)
            return 1;
    }
    username = username[0] ? username : nullptr;
    if (!issuer) {
        issuer = Prompt(T("Issuer:"), &temp_alloc);
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

    if (!skip_qrcode) {
        if (png_filename) {
            StreamWriter st(png_filename);
            if (!qr_EncodeTextToPng(url, 12, &st))
                return 1;
            if (!st.Close())
                return 1;

            LogInfo("QR code written to: %!..+%1%!0", png_filename);
        } else {
            LogInfo();

            bool ansi = FileIsVt100(STDOUT_FILENO);

            if (!qr_EncodeTextToBlocks(url, ansi, 2, StdOut))
                return 1;
        }
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

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 compute_totp [option...]%!0

Options:

    %!..+-a, --algorithm algorithm%!0      Change HMAC algorithm
                                   %!D..(default: %2)%!0
    %!..+-s, --secret secret%!0            Set secret in Base32 encoding

    %!..+-t, --time time%!0                Use specified Unix time instead of current time
    %!..+-d, --digits digits%!0            Generate specified number of digits
                                   %!D..(default: %3)%!0
    %!..+-w, --window window%!0            Generate multiple codes around current time
                                   %!D..(default: %4)%!0)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnumI(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
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

        opt.LogUnusedArguments();
    }

    if (!secret) {
        secret = Prompt(T("Secret:"), &temp_alloc);
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

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 check_totp [option...]%!0

Options:

    %!..+-a, --algorithm algorithm%!0      Change HMAC algorithm
                                   %!D..(default: %2)%!0
    %!..+-s, --secret secret%!0            Set secret in Base32 encoding

    %!..+-t, --time time%!0                Use specified Unix time instead of current time
    %!..+-d, --digits digits%!0            Generate specified number of digits
                                   %!D..(default: %3)%!0
    %!..+-w, --window window%!0            Generate multiple codes around current time
                                   %!D..(default: %4)%!0)",
                FelixTarget, pwd_HotpAlgorithmNames[(int)algorithm], digits, window);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-a", "--algorithm", OptionType::Value)) {
                if (!OptionToEnumI(pwd_HotpAlgorithmNames, opt.current_value, &algorithm)) {
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

        opt.LogUnusedArguments();
    }

    if (secret) {
        if (!pwd_CheckSecret(secret))
            return 1;
    } else {
        secret = Prompt(T("Secret:"), &temp_alloc);
        if (!secret)
            return 1;
        if (!secret[0]) {
            LogError("Secret must not be empty");
            return 1;
        }
    }

    if (!code) {
        PrintLn("Expecting %1 digits", digits);

        code = Prompt(T("Code:"), &temp_alloc);
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

static int RunEncodeQR(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *filename_or_text = nullptr;
    bool is_text = false;
    bool force_binary = false;
    const char *png_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 encode_qr [option...]%!0

Options:

    %!..+-F, --file filename%!0            Encode data from file
    %!..+-t, --text text%!0                Encode string passed as argument

         %!..+--force_binary%!0            Force use of binary encoding

    %!..+-P, --png_file filename%!0        Write QR code PNG image to disk)",
                FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-F", "--file", OptionType::Value)) {
                filename_or_text = opt.current_value;
                is_text = false;
            } else if (opt.Test("-t", "--text", OptionType::Value)) {
                filename_or_text = opt.current_value;
                is_text = true;
            } else if (opt.Test("--force_binary")) {
                force_binary = true;
            } else if (opt.Test("-P", "--png_file", OptionType::Value)) {
                png_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (filename_or_text && !is_text && TestStr(filename_or_text, "-")) {
        filename_or_text = nullptr;
    }

    // Load data
    HeapArray<uint8_t> data;
    if (is_text) {
        Span<const char> str = filename_or_text;
        data.Append(str.As<const uint8_t>());
    } else if (filename_or_text) {
        if (ReadFile(filename_or_text, Megabytes(2), &data) < 0)
            return 1;
    } else {
        if (StdIn->ReadAll(Megabytes(2), &data) < 0)
            return 1;
    }

    // Encode QR code
    if (png_filename) {
        StreamWriter st(png_filename);

        if (force_binary) {
            if (!qr_EncodeBinaryToPng(data, 12, &st))
                return 1;
        } else {
            if (!qr_EncodeTextToPng(data.As<const char>(), 12, &st))
                return 1;
        }
        if (!st.Close())
            return 1;

        LogInfo("QR code written to: %!..+%1%!0", png_filename);
    } else {
        bool ansi = FileIsVt100(STDOUT_FILENO);

        if (force_binary) {
            if (!qr_EncodeBinaryToBlocks(data, ansi, 2, StdOut))
                return 1;
        } else {
            if (!qr_EncodeTextToBlocks(data.As<const char>(), ansi, 2, StdOut))
                return 1;
        }
    }

    return 0;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0

Commands:

    %!..+generate_password%!0              Generate random password
    %!..+hash_password%!0                  Hash a password (using libsodium)
    %!..+check_password%!0                 Check password strength

    %!..+generate_totp%!0                  Generate a TOTP QR code
    %!..+compute_totp%!0                   Generate TOTP code based on current time
    %!..+check_totp%!0                     Check TOTP code based on current time

    %!..+encode_qr%!0                      Encode text or binary data as QR code

Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
        PrintLn(StdErr);
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
            print_usage(StdOut);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Execute relevant command
    if (TestStr(cmd, "generate_password")) {
        return RunGeneratePassword(arguments);
    } else if (TestStr(cmd, "hash_password")) {
        return RunHashPassword(arguments);
    } else if (TestStr(cmd, "check_password")) {
        return RunCheckPassword(arguments);
    } else if (TestStr(cmd, "generate_totp")) {
        return RunGenerateTOTP(arguments);
    } else if (TestStr(cmd, "compute_totp")) {
        return RunComputeTOTP(arguments);
    } else if (TestStr(cmd, "check_totp")) {
        return RunCheckTOTP(arguments);
    } else if (TestStr(cmd, "encode_qr")) {
        return RunEncodeQR(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
