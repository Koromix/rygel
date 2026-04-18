#!/bin/sh

set -e

../../../vendor/esbuild/native/bin/esbuild src/export.js --bundle --sourcemap=inline --platform=node --format=esm \
    --external:'../../../../vendor/*' --tsconfig-raw='{ "compilerOptions": { "paths": { "*": ["../../../*"] } } }' \
    --outfile=tmp/export.js
node ./tmp/export.js $@
