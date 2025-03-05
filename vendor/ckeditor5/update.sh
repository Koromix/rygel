#!/bin/sh -e

cd "$(dirname $0)"

npm install ckeditor5
npx esbuild --bundle --platform=browser --format=esm ckeditor5.js --outfile=ckeditor5.bundle.js

rm -rf package*.json
rm -rf node_modules
