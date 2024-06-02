#!/bin/sh -e

cd "$(dirname $0)"

npm install lit
npx esbuild --bundle --platform=browser --format=esm lit-html.js --outfile=lit-html.bundle.js

rm -rf package*.json
rm -rf node_modules
