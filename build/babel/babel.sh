#!/bin/sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

basedir=$(dirname "$(echo "$0" | sed -e 's,\\,/,g')")

cd "$basedir"
# npm install --silent --no-audit --no-optional --no-bin-links
node "node_modules/@babel/cli/bin/babel.js" --filename compat.js --config-file ./config.js "$@"
exit $?
