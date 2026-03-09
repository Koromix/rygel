#!/bin/sh

set -e

../../../vendor/esbuild/native/bin/esbuild src/export.js --bundle --sourcemap=inline --platform=node --format=esm \
    --external:'../../../../vendor/*' --external:better-sqlite3 --tsconfig-raw='{ "compilerOptions": { "paths": { "*": ["../../../*"] } } }' \
    --outfile=tmp/export.js
node ./tmp/export.js $@
