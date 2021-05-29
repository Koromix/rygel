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

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *password = nullptr;
    bool mask = true;
    bool confirm = true;
    bool check = true;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options]
       %1 -p <password>%!0

Options:
    %!..+-p, --password <password>%!0    Use password given as option

        %!..+--no_mask%!0                Show password as typed
        %!..+--no_confirm%!0             Ask only once for password
        %!..+--no_check%!0               Don't check password strength)", FelixTarget);
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
            } else if (opt.Test("-p", "--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--no_mask")) {
                mask = false;
            } else if (opt.Test("--no_confirm")) {
                confirm = false;
            } else if (opt.Test("--no_check")) {
                check = false;
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

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
