#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

npm install "@zip.js/zip.js@$VERSION"
npx esbuild --bundle --platform=browser --format=esm zip.js --outfile=zip.bundle.js

rm -rf package*.json
rm -rf node_modules
