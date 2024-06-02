#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

npm install https://cdn.sheetjs.com/xlsx-$VERSION/xlsx-$VERSION.tgz
npx esbuild --bundle --platform=browser --format=esm XLSX.js --outfile=XLSX.bundle.js

rm -rf package*.json
rm -rf node_modules
