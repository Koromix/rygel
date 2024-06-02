#!/bin/sh -e

cd "$(dirname $0)"

npm install chart.js
npx esbuild --bundle --platform=browser --format=esm chart.js --outfile=chart.bundle.js

rm -rf package*.json
rm -rf node_modules
