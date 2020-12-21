// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"

#include "../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *password = nullptr;
    bool mask = true;
    bool confirm = true;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options]
       %1 -p <password>%!0

Options:
    %!..+-p, --password <password>%!0    Use password given as option

        %!..+--no_mask%!0                Show password as typed
        %!..+--no_confirm%!0             Ask only once for password)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
        return 0;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-p", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--no_mask")) {
                mask = false;
            } else if (opt.Test("--no_confirm")) {
                confirm = false;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    if (!password) {
        password = Prompt("Password: ", mask ? "*" : nullptr, &temp_alloc);
        if (!password)
            return 1;
        if (!password[0]) {
            LogError("Password must not be empty");
            return 1;
        }

        if (confirm) {
            const char *password2 = Prompt("Confirm: ", mask ? "*" : nullptr, &temp_alloc);
            if (!password2)
                return 1;

            if (!TestStr(password, password2)) {
                LogError("Password mismatch");
                return 1;
            }
        }
    } else if (!password[0]) {
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

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
