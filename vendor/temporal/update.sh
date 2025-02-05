#!/bin/sh -e

cd "$(dirname $0)"

npm install temporal-polyfill
npx esbuild --bundle --platform=browser --format=esm temporal.js --outfile=temporal.bundle.js

rm -rf package*.json
rm -rf node_modules
