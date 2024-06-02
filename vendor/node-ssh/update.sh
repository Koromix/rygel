#!/bin/sh -e

cd "$(dirname $0)"

npm install --omit=optional node-ssh
npx esbuild --bundle --platform=node --loader:.node=empty --format=cjs node-ssh.js --outfile=node-ssh.bundle.js

rm -rf package*.json
rm -rf node_modules
