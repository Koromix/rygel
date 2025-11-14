#!/bin/sh

set -e

script=$1
name=$(printf "%s" "$script" | sha256sum | cut -d ' ' -f 1)
shift

./node_modules/.bin/esbuild $script --bundle --sourcemap=inline --platform=node \
    --external:esbuild --external:better-sqlite3 --tsconfig-raw="{ \"compilerOptions\": { \"baseUrl\": \"../..\" } }" \
    --outfile=tmp/$name.js
node ./tmp/$name.js $@
