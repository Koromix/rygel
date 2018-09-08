#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import nacl.pwhash

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Hash password with libsodium')
    parser.add_argument('password', metavar = 'password', type = str)
    args = parser.parse_args()

    hash = nacl.pwhash.str(args.password.encode('utf-8'),
                           opslimit = nacl.pwhash.OPSLIMIT_INTERACTIVE,
                           memlimit = nacl.pwhash.MEMLIMIT_INTERACTIVE)
    print('PasswordHash =', hash.decode())
