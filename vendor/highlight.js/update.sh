#!/bin/sh -e

cd "$(dirname $0)"

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Missing version argument" >&2
    exit 1
fi

git clone https://github.com/highlightjs/highlight.js repo

cd repo
git checkout "$VERSION"
npm install
npm run build-browser :common

cd ..
npx esbuild --bundle --platform=browser --format=esm ./highlight.js --outfile=highlight.bundle.js
rsync -rtvp repo/build/demo/styles/ styles/ --delete

rm -rf repo
