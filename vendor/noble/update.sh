#!/bin/sh -e

cd "$(dirname $0)"

npm install @noble/hashes
npm install @serenity-kit/noble-sodium

npx esbuild --bundle --platform=browser --format=esm noble.js --outfile=noble.bundle.js

rm -rf package*.json
rm -rf node_modules
