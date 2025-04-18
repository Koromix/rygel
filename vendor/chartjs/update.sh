#!/bin/sh -e

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

cd "$(dirname $0)"

npm install "chart.js@$VERSION"
npx esbuild --bundle --platform=browser --format=esm chart.js --outfile=chart.bundle.js

rm -rf package*.json
rm -rf node_modules
