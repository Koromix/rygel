#!/bin/sh -e

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

cd "$(dirname $0)"

npm install "@awasm/noble@$VERSION"

npx esbuild --bundle --platform=browser --format=esm awasm-noble.js --outfile=awasm-noble.bundle.js

rm -rf package*.json
rm -rf node_modules
