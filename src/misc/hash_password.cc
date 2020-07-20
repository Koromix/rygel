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
    Span <const char> password = {};

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: gen_password_hash
       gen_password_hash -p <password>

Options:
    -p, --password <password>    Use password given as option)");
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-p", OptionType::Value)) {
                password = opt.current_value;
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

    if (!password.ptr) {
        password = Prompt("Password: ", "*", &temp_alloc);
        if (!password.len)
            return 1;
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password.ptr, password.len,
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
